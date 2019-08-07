// copyright defined in LICENSE.txt

#include "fill_rocksdb_plugin.hpp"
#include "state_history_connection.hpp"
#include "state_history_rocksdb.hpp"
#include "util.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <fc/exception/exception.hpp>

using namespace abieos;
using namespace appbase;
using namespace std::literals;
using namespace state_history;

namespace asio      = boost::asio;
namespace bpo       = boost::program_options;
namespace kv        = state_history::kv;
namespace rdb       = state_history::rdb;
namespace websocket = boost::beast::websocket;

using asio::ip::tcp;
using boost::beast::flat_buffer;
using boost::system::error_code;

struct flm_session;

struct rocksdb_table;

struct rocksdb_field {
    std::string                    name        = {};
    const abieos::abi_field*       abi_field   = {};
    const kv::type*                type        = {};
    std::unique_ptr<rocksdb_table> array_of    = {};
    std::unique_ptr<rocksdb_table> optional_of = {};
};

struct rocksdb_table {
    std::string                                 name      = {};
    kv::table*                                  kv_table  = {};
    const abieos::abi_type*                     abi_type  = {};
    std::vector<std::unique_ptr<rocksdb_field>> fields    = {};
    std::map<std::string, rocksdb_field*>       field_map = {};
};

struct fill_rocksdb_config : connection_config {
    uint32_t                skip_to      = 0;
    uint32_t                stop_before  = 0;
    std::vector<trx_filter> trx_filters  = {};
    bool                    enable_trim  = false;
    bool                    enable_check = false;
};

struct fill_rocksdb_plugin_impl : std::enable_shared_from_this<fill_rocksdb_plugin_impl> {
    std::shared_ptr<fill_rocksdb_config> config = std::make_shared<fill_rocksdb_config>();
    std::shared_ptr<::flm_session>       session;
    boost::asio::deadline_timer          timer;

    fill_rocksdb_plugin_impl()
        : timer(app().get_io_service()) {}

    ~fill_rocksdb_plugin_impl();

    void schedule_retry() {
        timer.expires_from_now(boost::posix_time::seconds(1));
        timer.async_wait([this](auto&) {
            ilog("retry...");
            start();
        });
    }

    void start();
};

struct flm_session : connection_callbacks, std::enable_shared_from_this<flm_session> {
    fill_rocksdb_plugin_impl*                  my = nullptr;
    std::shared_ptr<fill_rocksdb_config>       config;
    std::shared_ptr<::rocksdb_inst>            rocksdb_inst = app().find_plugin<rocksdb_plugin>()->get_rocksdb_inst_rw();
    rocksdb::WriteBatch                        active_content_batch;
    rocksdb::WriteBatch                        active_index_batch;
    std::shared_ptr<state_history::connection> connection;
    std::map<std::string, rocksdb_table>       tables             = {};
    rocksdb_table*                             block_info_table   = {};
    rocksdb_table*                             action_trace_table = {};
    std::optional<state_history::fill_status>  current_db_status  = {};
    uint32_t                                   head               = 0;
    abieos::checksum256                        head_id            = {};
    uint32_t                                   irreversible       = 0;
    abieos::checksum256                        irreversible_id    = {};
    uint32_t                                   first              = 0;

    flm_session(fill_rocksdb_plugin_impl* my)
        : my(my)
        , config(my->config) {}

    void connect(asio::io_context& ioc) {
        connection = std::make_shared<state_history::connection>(ioc, *config, shared_from_this());
        connection->connect();
    }

    void check() {
        rocksdb_inst->database.flush(true, true);
        ilog("checking database");
        load_fill_status();
        if (!current_db_status) {
            ilog("database is empty");
            return;
        }
        ilog("first: ${first}, irreversible: ${irreversible}, head: ${head}", ("first", first)("irreversible", irreversible)("head", head));

        ilog("verifying expected records are present");
        uint32_t expected = first;

        for_each_subkey(rocksdb_inst->database, kv::make_table_key(0), kv::make_table_key(0xffff'ffff), [&](auto&, auto k, auto) {
            auto orig_k = k;
            if (kv::bin_to_key_tag(k) != kv::key_tag::table)
                throw std::runtime_error("This shouldn't happen (1)");
            auto block_num = kv::key_to_native<uint32_t>(k);
            for_each_subkey(
                rocksdb_inst->database, kv::make_table_key(block_num, false, "recvd.block"_n),
                kv::make_table_key(block_num, true, "recvd.block"_n), [&](auto&, auto k, auto) {
                    if (block_num != 0 && (block_num < first || block_num > head))
                        throw std::runtime_error(
                            "Saw row for block_num " + std::to_string(block_num) +
                            ", which is out of range [first, head]. key: " + kv::key_to_string(orig_k));
                    if (block_num == first || block_num == head || !(block_num % 10'000))
                        ilog("found received_block ${b}", ("b", block_num));
                    if (block_num != expected)
                        throw std::runtime_error(
                            "Saw received_block record " + std::to_string(block_num) + " but expected " + std::to_string(expected));
                    ++expected;
                    return true;
                });
            return true;
        });
        ilog("found received_block ${b}", ("b", expected - 1));
        if (expected - 1 != head)
            throw std::runtime_error("Found head " + std::to_string(expected - 1) + " but fill_status.head = " + std::to_string(head));

        ilog("verifying index entries reference existing records");
        uint64_t     num_ti_keys = 0;
        abieos::name last_table, last_index;
        uint64_t     last_num_keys = 0;
        for_each(rocksdb_inst->database, kv::make_index_key(), kv::make_index_key(), [&](auto k, auto v) {
            abieos::name table, index;
            auto         kk = k;
            kv::key_to_native<uint8_t>(kk);
            kv::read_index_prefix(kk, table, index);
            if (table != last_table || index != last_index) {
                if (last_table.value)
                    ilog(
                        "table '${t}' index '${i}' has ${e} entries",
                        ("t", (std::string)last_table)("i", (std::string)last_index)("e", last_num_keys));
                last_table    = table;
                last_index    = index;
                last_num_keys = 0;
                ilog("table '${t}' index '${i}'", ("t", (std::string)last_table)("i", (std::string)last_index));
            }

            ++last_num_keys;
            if (!((++num_ti_keys) % 1'000'000))
                ilog("found ${n} index entries so far, ${i} for this index", ("n", num_ti_keys)("i", last_num_keys));

            auto& c        = rocksdb_inst->query_config;
            auto  index_it = c.index_name_map.find(index);
            if (index_it == c.index_name_map.end())
                throw std::runtime_error("found unknown index '" + (std::string)index + "'");
            auto& index_obj = *index_it->second;
            if (index_obj.table_obj->short_name != table)
                throw std::runtime_error("index '" + (std::string)index + "' is not for table '" + (std::string)table + "'");

            auto pk = extract_pk_from_index(k, *index_obj.table_obj, index_obj.sort_keys);
            if (!rdb::exists(rocksdb_inst->database, rdb::to_slice(pk)))
                throw std::runtime_error(
                    "index '" + (std::string)index + "' references a missing entry in table '" + (std::string)table + "'");
            return true;
        });
        ilog(
            "table '${t}' index '${i}' has ${e} entries", ("t", (std::string)last_table)("i", (std::string)last_index)("e", last_num_keys));
        ilog("checked ${n} index entries", ("n", num_ti_keys));
        ilog("database appears ok");
    }

    void fill_fields(rocksdb_table& table, const std::string& base_name, const abieos::abi_field& abi_field) {
        if (abi_field.type->filled_struct) {
            for (auto& f : abi_field.type->fields)
                fill_fields(table, base_name + abi_field.name + "_", f);
        } else if (abi_field.type->filled_variant && abi_field.type->fields.size() == 1 && abi_field.type->fields[0].type->filled_struct) {
            for (auto& f : abi_field.type->fields[0].type->fields)
                fill_fields(table, base_name + abi_field.name + "_", f);
        } else {
            bool array_of_struct  = abi_field.type->array_of && abi_field.type->array_of->filled_struct;
            bool array_of_variant = abi_field.type->array_of && abi_field.type->array_of->filled_variant &&
                                    abi_field.type->array_of->fields.size() == 1 && abi_field.type->array_of->fields[0].type->filled_struct;
            bool optional_of_struct = abi_field.type->optional_of && abi_field.type->optional_of->filled_struct;
            auto field_name         = base_name + abi_field.name;
            if (table.field_map.find(field_name) != table.field_map.end())
                throw std::runtime_error("duplicate field " + field_name + " in table " + table.name);

            auto* raw_type = abi_field.type;
            if (raw_type->optional_of)
                raw_type = raw_type->optional_of;
            if (raw_type->array_of)
                raw_type = raw_type->array_of;
            auto type_it = kv::abi_type_to_kv_type.find(raw_type->name);
            if (type_it == kv::abi_type_to_kv_type.end() && !array_of_struct && !array_of_variant && !optional_of_struct)
                throw std::runtime_error("don't know rocksdb type for abi type: " + raw_type->name);

            table.fields.push_back(std::make_unique<rocksdb_field>());
            auto* f                     = table.fields.back().get();
            table.field_map[field_name] = f;
            f->name                     = field_name;
            f->abi_field                = &abi_field;
            f->type                     = (array_of_struct | array_of_variant | optional_of_struct) ? nullptr : &type_it->second;

            if (array_of_struct) {
                f->array_of = std::make_unique<rocksdb_table>(rocksdb_table{.name = field_name, .abi_type = abi_field.type->array_of});
                for (auto& g : abi_field.type->array_of->fields)
                    fill_fields(*f->array_of, base_name, g);
            } else if (array_of_variant) {
                f->array_of = std::make_unique<rocksdb_table>(
                    rocksdb_table{.name = field_name, .abi_type = abi_field.type->array_of->fields[0].type});
                for (auto& g : abi_field.type->array_of->fields[0].type->fields)
                    fill_fields(*f->array_of, base_name, g);
            } else if (optional_of_struct) {
                f->optional_of =
                    std::make_unique<rocksdb_table>(rocksdb_table{.name = field_name, .abi_type = abi_field.type->optional_of});
                for (auto& g : abi_field.type->optional_of->fields)
                    fill_fields(*f->optional_of, base_name, g);
            }
        }
    }

    rocksdb_table& get_table(const std::string& name) {
        auto table_it = tables.find(name);
        if (table_it == tables.end())
            throw std::runtime_error("unknown table \"" + name + "\"");
        return table_it->second;
    }

    kv::table& get_kv_table(const std::string& name) {
        auto& c  = rocksdb_inst->query_config;
        auto  it = c.table_map.find(name);
        if (it == c.table_map.end())
            throw std::runtime_error("table \"" + name + "\" missing in query-config");
        return *it->second;
    }

    kv::table& get_kv_table(abieos::name name) {
        auto& c  = rocksdb_inst->query_config;
        auto  it = c.table_name_map.find(name);
        if (it == c.table_name_map.end())
            throw std::runtime_error("table \"" + (std::string)name + "\" missing in query-config");
        return *it->second;
    }

    void add_table(const std::string& table_name, const std::string& table_type, const jarray& key_names) {
        if (tables.find(table_name) != tables.end())
            throw std::runtime_error("duplicate table \"" + table_name + "\"");

        auto& table    = tables[table_name];
        table.name     = table_name;
        table.kv_table = &get_kv_table(table_name);
        table.abi_type = &get_type(table_type);

        if (!table.abi_type->filled_variant || table.abi_type->fields.size() != 1 || !table.abi_type->fields[0].type->filled_struct)
            throw std::runtime_error("don't know how to process " + table.abi_type->name);

        for (auto& f : table.abi_type->fields[0].type->fields)
            fill_fields(table, "", f);
    }

    void init_tables(std::string_view abi_json) {
        std::string error;
        jvalue      j;
        if (!json_to_jvalue(j, error, abi_json))
            throw std::runtime_error(error);
        for (auto& t : std::get<jarray>(std::get<jobject>(j.value)["tables"].value)) {
            auto& o = std::get<jobject>(t.value);
            add_table(
                std::get<std::string>(o["name"].value), std::get<std::string>(o["type"].value), std::get<jarray>(o["key_names"].value));
        }

        block_info_table           = &tables["block_info"];
        block_info_table->name     = "block_info";
        block_info_table->kv_table = &get_kv_table("block_info");
        fill_fields(*block_info_table, "", abieos::abi_field{"block_num", &get_type("uint32")});
        fill_fields(*block_info_table, "", abieos::abi_field{"block_id", &get_type("checksum256")});
        fill_fields(*block_info_table, "", abieos::abi_field{"timestamp", &get_type("block_timestamp_type")});
        fill_fields(*block_info_table, "", abieos::abi_field{"producer", &get_type("name")});
        fill_fields(*block_info_table, "", abieos::abi_field{"confirmed", &get_type("uint16")});
        fill_fields(*block_info_table, "", abieos::abi_field{"previous", &get_type("checksum256")});
        fill_fields(*block_info_table, "", abieos::abi_field{"transaction_mroot", &get_type("checksum256")});
        fill_fields(*block_info_table, "", abieos::abi_field{"action_mroot", &get_type("checksum256")});
        fill_fields(*block_info_table, "", abieos::abi_field{"schedule_version", &get_type("uint32")});

        action_trace_table           = &tables["action_trace"];
        action_trace_table->name     = "action_trace";
        action_trace_table->kv_table = &get_kv_table("action_trace");
        fill_fields(*action_trace_table, "", abieos::abi_field{"block_num", &get_type("uint32")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"transaction_id", &get_type("checksum256")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"transaction_status", &get_type("uint8")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"action_ordinal", &get_type("varuint32")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"creator_action_ordinal", &get_type("varuint32")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"receiver", &get_type("name")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"act_account", &get_type("name")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"act_name", &get_type("name")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"act_data", &get_type("bytes")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"context_free", &get_type("bool")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"elapsed", &get_type("int64")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"console", &get_type("string")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"except", &get_type("string")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"error_code", &get_type("uint64")});

        if (config->enable_trim) {
            auto& c = rocksdb_inst->query_config;
            for (auto& table : c.tables) {
                if (table.is_delta && !table.trim_index_obj)
                    throw std::runtime_error("delta table " + table.name + " is missing a trim index");
                if (!table.is_delta && table.trim_index_obj)
                    throw std::runtime_error("non-delta table " + table.name + " has a trim index");
            }
        }
    } // init_tables

    void received_abi(std::string_view abi) override {
        init_tables(abi);

        load_fill_status();
        ilog("clean up stale records");
        end_write(true);
        truncate(head + 1);
        end_write(true);
        rocksdb_inst->database.flush(true, true);

        if (config->enable_check)
            check();

        ilog("request status");
        connection->send(get_status_request_v0{});
    }

    bool received(get_status_result_v0& status) override {
        ilog("request blocks");
        connection->request_blocks(status, std::max(config->skip_to, head + 1), get_positions());
        return true;
    }

    void load_fill_status() {
        current_db_status = rdb::get<state_history::fill_status>(rocksdb_inst->database, kv::make_fill_status_key(), false);
        if (!current_db_status)
            return;
        head            = current_db_status->head;
        head_id         = current_db_status->head_id;
        irreversible    = current_db_status->irreversible;
        irreversible_id = current_db_status->irreversible_id;
        first           = current_db_status->first;
    }

    std::vector<block_position> get_positions() {
        std::vector<block_position> result;
        if (head) {
            for (uint32_t i = irreversible; i <= head; ++i) {
                auto rb = rdb::get<kv::received_block>(rocksdb_inst->database, kv::make_received_block_key(i), true);
                result.push_back({rb->block_num, rb->block_id});
            }
        }
        return result;
    }

    void write_fill_status(rocksdb::WriteBatch& batch) {
        if (irreversible < head)
            current_db_status = state_history::fill_status{
                .head = head, .head_id = head_id, .irreversible = irreversible, .irreversible_id = irreversible_id, .first = first};
        else
            current_db_status = state_history::fill_status{
                .head = head, .head_id = head_id, .irreversible = head, .irreversible_id = head_id, .first = first};
        rdb::put(batch, kv::make_fill_status_key(), *current_db_status, true);
    }

    void truncate(uint32_t block) {
        rocksdb_inst->database.flush(true, true);
        rocksdb::WriteBatch content_batch, index_batch;
        uint64_t            num_rows    = 0;
        uint64_t            num_indexes = 0;
        for_each(rocksdb_inst->database, kv::make_table_key(block), kv::make_table_key(), [&](auto k, auto v) {
            remove_row(content_batch, index_batch, k, v, &num_rows, &num_indexes);
            return true;
        });

        auto rb = rdb::get<kv::received_block>(rocksdb_inst->database, kv::make_received_block_key(block - 1), false);
        if (!rb) {
            head    = 0;
            head_id = {};
        } else {
            head    = block - 1;
            head_id = rb->block_id;
        }
        first = std::min(first, head);

        // todo: should fill_status be written first?

        // erase indexes before content
        write(rocksdb_inst->database, index_batch);
        write(rocksdb_inst->database, content_batch);

        ilog("removed ${r} rows and ${i} index entries", ("r", num_rows)("i", num_indexes));
    }

    void end_write(bool write_fill) {
        if (write_fill)
            write_fill_status(active_index_batch);

        // write content before indexes to enable truncate() to behave correctly if process exits before flushing
        write(rocksdb_inst->database, active_content_batch);
        write(rocksdb_inst->database, active_index_batch);
    }

    bool received(get_blocks_result_v0& result) override {
        if (!result.this_block)
            return true;
        if (config->stop_before && result.this_block->block_num >= config->stop_before) {
            ilog("block ${b}: stop requested", ("b", result.this_block->block_num));
            end_write(true);
            rocksdb_inst->database.flush(false, false);
            return false;
        }

        /*
        if (result.this_block->block_num >= 40013524) {
            end_write(true);
            {
                std::string ss;
                rocksdb_inst->database.db->GetProperty("rocksdb.stats", &ss);
                std::cout << ss << "\n";
            }
            rocksdb_inst->database.flush(true, true);
            {
                std::string ss;
                rocksdb_inst->database.db->GetProperty("rocksdb.stats", &ss);
                std::cout << ss << "\n";
            }
            _exit(0);
        }
        */

        try {
            if (result.this_block->block_num <= head) {
                ilog("switch forks at block ${b}", ("b", result.this_block->block_num));
                end_write(true);
                truncate(result.this_block->block_num);
                end_write(true);
            }

            bool near       = result.this_block->block_num + 4 >= result.last_irreversible.block_num;
            bool commit_now = !(result.this_block->block_num % 200) || near;
            if (commit_now)
                ilog("block ${b}", ("b", result.this_block->block_num));

            if (head_id != abieos::checksum256{} && (!result.prev_block || result.prev_block->block_id != head_id))
                throw std::runtime_error("prev_block does not match");
            if (result.block)
                receive_block(
                    result.this_block->block_num, result.this_block->block_id, *result.block, active_content_batch, active_index_batch);
            if (result.deltas)
                receive_deltas(active_content_batch, active_index_batch, result.this_block->block_num, *result.deltas);
            if (result.traces)
                receive_traces(active_content_batch, active_index_batch, result.this_block->block_num, *result.traces);

            head            = result.this_block->block_num;
            head_id         = result.this_block->block_id;
            irreversible    = result.last_irreversible.block_num;
            irreversible_id = result.last_irreversible.block_id;
            if (!first)
                first = head;

            rdb::put(
                active_content_batch, kv::make_received_block_key(result.this_block->block_num),
                kv::received_block{result.this_block->block_num, result.this_block->block_id});

            if (commit_now) {
                end_write(true);
                if (config->enable_trim)
                    trim();
            }
            if (near)
                rocksdb_inst->database.flush(false, false);
        } catch (...) {
            throw;
        }

        return true;
    } // receive_result()

    void fill(std::vector<char>& dest, input_buffer& src, rocksdb_field& field) {
        if (field.abi_field->type->filled_variant && field.abi_field->type->fields.size() == 1 &&
            field.abi_field->type->fields[0].type->filled_struct) {
            auto v = read_varuint32(src);
            if (v)
                throw std::runtime_error("invalid variant in " + field.abi_field->type->name);
            abieos::push_varuint32(dest, v);
        } else if (field.optional_of) {
            bool b = read_raw<bool>(src);
            abieos::push_raw(dest, b);
            if (b) {
                for (auto& f : field.optional_of->fields)
                    fill(dest, src, *f);
            }
        } else if (field.array_of) {
            uint32_t n = read_varuint32(src);
            abieos::push_varuint32(dest, n);
            for (uint32_t i = 0; i < n; ++i) {
                for (auto& f : field.array_of->fields)
                    fill(dest, src, *f);
            }
        } else {
            if (!field.type->bin_to_bin)
                throw std::runtime_error("don't know how to process " + field.abi_field->type->name);
            if (field.abi_field->type->optional_of) {
                bool exists = read_raw<bool>(src);
                abieos::push_raw<bool>(dest, exists);
                if (!exists)
                    return;
            }
            field.type->bin_to_bin(dest, src);
        }
    } // fill

    void add_row(
        rocksdb::WriteBatch& content_batch, rocksdb::WriteBatch& index_batch, rocksdb_table& table, uint32_t block_num, bool present_k,
        const std::vector<char>& value) {
        kv::clear_positions(table.kv_table->fields);
        kv::fill_positions({value.data(), value.data() + value.size()}, table.kv_table->fields);

        std::vector<char> key;
        kv::append_table_key(key, block_num, present_k, table.kv_table->short_name);
        kv::extract_keys_from_value(key, {value.data(), value.data() + value.size()}, table.kv_table->keys);
        rdb::put(content_batch, key, value);

        std::vector<char> index_key;
        for (auto* index : table.kv_table->indexes) {
            if (index->only_for_trim) // temp: disable trim indexes
                continue;
            index_key.clear();
            kv::append_index_key(index_key, table.kv_table->short_name, index->short_name);
            kv::extract_keys_from_value(index_key, {value.data(), value.data() + value.size()}, index->sort_keys);
            kv::append_index_suffix(index_key, block_num, present_k);
            index_batch.Put(rdb::to_slice(index_key), {});
        }
    }

    void remove_row(
        rocksdb::WriteBatch& content_batch, rocksdb::WriteBatch& index_batch, abieos::input_buffer k, abieos::input_buffer v,
        uint64_t* num_rows = nullptr, uint64_t* num_indexes = nullptr) {
        uint32_t     block_num;
        abieos::name table_name;
        bool         present_k;
        auto         temp_k = k;
        kv::key_to_native<uint8_t>(temp_k);
        kv::read_table_prefix(temp_k, block_num, table_name, present_k);

        auto& table = get_kv_table(table_name);
        kv::clear_positions(table.fields);
        kv::fill_positions(v, table.fields);

        std::vector<char> index_key;
        for (auto* index : table.indexes) {
            index_key.clear();
            kv::append_index_key(index_key, table_name, index->short_name);
            kv::extract_keys_from_value(index_key, v, index->sort_keys);
            kv::append_index_suffix(index_key, block_num, present_k);
            index_batch.Delete(rdb::to_slice(index_key));
            if (num_indexes)
                ++*num_indexes;
        }

        content_batch.Delete(rdb::to_slice(k));
        if (num_rows)
            ++*num_rows;
    }

    void remove_row(
        rocksdb::WriteBatch& content_batch, rocksdb::WriteBatch& index_batch, abieos::input_buffer k, uint64_t* num_rows = nullptr,
        uint64_t* num_indexes = nullptr) {

        rocksdb::PinnableSlice v;
        auto*                  db   = rocksdb_inst->database.db.get();
        auto                   stat = db->Get(rocksdb::ReadOptions(), db->DefaultColumnFamily(), rdb::to_slice(k), &v);
        rdb::check(stat, "get: ");
        remove_row(content_batch, index_batch, k, rdb::to_input_buffer(v), num_rows, num_indexes);
    }

    void receive_block(
        uint32_t block_num, const checksum256& block_id, input_buffer bin, rocksdb::WriteBatch& content_batch,
        rocksdb::WriteBatch& index_batch) {
        state_history::signed_block block;
        bin_to_native(block, bin);
        std::vector<char> value;

        abieos::native_to_bin(block_num, value);
        abieos::native_to_bin(block_id, value);
        abieos::native_to_bin(block.timestamp, value);
        abieos::native_to_bin(block.producer, value);
        abieos::native_to_bin(block.confirmed, value);
        abieos::native_to_bin(block.previous, value);
        abieos::native_to_bin(block.transaction_mroot, value);
        abieos::native_to_bin(block.action_mroot, value);
        abieos::native_to_bin(block.schedule_version, value);
        abieos::native_to_bin(block.new_producers ? *block.new_producers : state_history::producer_schedule{}, value);

        add_row(content_batch, index_batch, get_table("block_info"), block_num, true, value);
    } // receive_block

    void receive_deltas(rocksdb::WriteBatch& content_batch, rocksdb::WriteBatch& index_batch, uint32_t block_num, input_buffer bin) {
        auto&             table_delta_type = get_type("table_delta");
        std::vector<char> value;

        auto num = read_varuint32(bin);
        for (uint32_t i = 0; i < num; ++i) {
            check_variant(bin, table_delta_type, "table_delta_v0");
            state_history::table_delta_v0 table_delta;
            bin_to_native(table_delta, bin);
            auto& table = get_table(table_delta.name);

            size_t num_processed = 0;
            for (auto& row : table_delta.rows) {
                if (table_delta.rows.size() > 10000 && !(num_processed % 10000)) {
                    ilog(
                        "block ${b} ${t} ${n} of ${r}",
                        ("b", block_num)("t", table_delta.name)("n", num_processed)("r", table_delta.rows.size()));
                    end_write(false);
                }
                check_variant(row.data, *table.abi_type, 0u);
                value.clear();
                abieos::native_to_bin(block_num, value);
                abieos::native_to_bin(row.present, value);
                for (auto& field : table.fields)
                    fill(value, row.data, *field);
                add_row(content_batch, index_batch, table, block_num, row.present, value);
                ++num_processed;
            }
        }
    } // receive_deltas

    void receive_traces(rocksdb::WriteBatch& content_batch, rocksdb::WriteBatch& index_batch, uint32_t block_num, input_buffer bin) {
        auto     num          = read_varuint32(bin);
        uint32_t num_ordinals = 0;
        for (uint32_t i = 0; i < num; ++i) {
            state_history::transaction_trace trace;
            bin_to_native(trace, bin);
            if (filter(config->trx_filters, std::get<0>(trace)))
                write_transaction_trace(
                    content_batch, index_batch, block_num, num_ordinals, std::get<state_history::transaction_trace_v0>(trace));
        }
    }

    void write_transaction_trace(
        rocksdb::WriteBatch& content_batch, rocksdb::WriteBatch& index_batch, uint32_t block_num, uint32_t& num_ordinals,
        const state_history::transaction_trace_v0& ttrace) {
        auto* failed = !ttrace.failed_dtrx_trace.empty()
                           ? &std::get<state_history::transaction_trace_v0>(ttrace.failed_dtrx_trace[0].recurse)
                           : nullptr;
        if (failed) {
            if (!filter(config->trx_filters, *failed))
                return;
            write_transaction_trace(content_batch, index_batch, block_num, num_ordinals, *failed);
        }
        uint32_t transaction_ordinal = ++num_ordinals;

        std::vector<char> key;
        kv::append_transaction_trace_key(key, block_num, ttrace.id);

        std::vector<char> value;
        abieos::native_to_bin(block_num, value);
        abieos::native_to_bin(transaction_ordinal, value);
        abieos::native_to_bin(failed ? failed->id : abieos::checksum256{}, value);
        abieos::native_to_bin(ttrace.id, value);
        abieos::native_to_bin((uint8_t)ttrace.status, value);
        abieos::native_to_bin(ttrace.cpu_usage_us, value);
        abieos::native_to_bin(ttrace.net_usage_words, value);
        abieos::native_to_bin(ttrace.elapsed, value);
        abieos::native_to_bin(ttrace.net_usage, value);
        abieos::native_to_bin(ttrace.scheduled, value);
        abieos::native_to_bin(ttrace.account_ram_delta.has_value(), value);
        if (ttrace.account_ram_delta) {
            abieos::native_to_bin(ttrace.account_ram_delta->account, value);
            abieos::native_to_bin(ttrace.account_ram_delta->delta, value);
        }
        abieos::native_to_bin(ttrace.except ? *ttrace.except : "", value);
        abieos::native_to_bin(ttrace.error_code ? *ttrace.error_code : 0, value);

        // rdb::put(batch, key, value); // todo: indexes, including trim

        for (auto& atrace : ttrace.action_traces)
            write_action_trace(content_batch, index_batch, block_num, ttrace, std::get<state_history::action_trace_v0>(atrace), value);
    }

    void write_action_trace(
        rocksdb::WriteBatch& content_batch, rocksdb::WriteBatch& index_batch, uint32_t block_num,
        const state_history::transaction_trace_v0& ttrace, const state_history::action_trace_v0& atrace, std::vector<char>& value) {
        value.clear();

        abieos::native_to_bin(block_num, value);
        abieos::native_to_bin(ttrace.id, value);
        abieos::native_to_bin((uint8_t)ttrace.status, value);
        abieos::native_to_bin(atrace.action_ordinal, value);
        abieos::native_to_bin(atrace.creator_action_ordinal, value);
        abieos::native_to_bin(atrace.receipt.has_value(), value);
        if (atrace.receipt) {
            auto& receipt = std::get<state_history::action_receipt_v0>(*atrace.receipt);
            abieos::native_to_bin(receipt.receiver, value);
            abieos::native_to_bin(receipt.act_digest, value);
            abieos::native_to_bin(receipt.global_sequence, value);
            abieos::native_to_bin(receipt.recv_sequence, value);
            abieos::native_to_bin(receipt.code_sequence, value);
            abieos::native_to_bin(receipt.abi_sequence, value);
        }
        abieos::native_to_bin(atrace.receiver, value);
        abieos::native_to_bin(atrace.act.account, value);
        abieos::native_to_bin(atrace.act.name, value);
        abieos::native_to_bin(atrace.act.data, value);
        abieos::native_to_bin(atrace.context_free, value);
        abieos::native_to_bin(atrace.elapsed, value);
        abieos::native_to_bin(atrace.console, value);
        abieos::native_to_bin(atrace.except ? *atrace.except : "", value);
        abieos::native_to_bin(atrace.error_code ? *atrace.error_code : 0, value);

        abieos::native_to_bin(atrace.console, value);
        abieos::native_to_bin(atrace.except ? *atrace.except : std::string(), value);

        add_row(content_batch, index_batch, get_table("action_trace"), block_num, true, value);

        // todo: receipt_auth_sequence
        // todo: authorization
        // todo: account_ram_deltas
    }

    void trim() {
        auto end_trim = std::min(head, irreversible);
        if (first >= end_trim)
            return;
        rocksdb_inst->database.flush(true, true);
        rocksdb::WriteBatch batch;
        ilog("trim: ${b} - ${e}", ("b", first)("e", end_trim));

        uint64_t                    num_rows    = 0;
        uint64_t                    num_indexes = 0;
        std::set<std::vector<char>> trim_keys;

        auto lower_bound = kv::make_table_key(first);
        auto upper_bound = kv::make_table_key(end_trim);
        rdb::for_each(rocksdb_inst->database, lower_bound, upper_bound, [&](auto k, auto v) {
            uint32_t     block_num;
            abieos::name table_name;
            bool         present_k;
            auto         temp_k = k;
            kv::key_to_native<uint8_t>(temp_k);
            kv::read_table_prefix(temp_k, block_num, table_name, present_k);

            auto& table = get_kv_table(table_name);
            if (table.trim_index_obj && block_num > first) {
                std::vector<char> index_key;
                kv::clear_positions(table.fields);
                kv::fill_positions(v, table.fields);
                kv::append_index_key(index_key, table_name, table.trim_index_obj->short_name);
                kv::extract_keys_from_value(index_key, v, table.trim_index_obj->sort_keys);
                trim_keys.insert(std::move(index_key));
            } else if (!table.trim_index_obj && block_num < end_trim) {
                remove_row(batch, batch, k, v, &num_rows, &num_indexes);
            }
            return true;
        });

        for (auto& range : trim_keys) {
            abieos::name         table_name;
            abieos::name         index_name;
            abieos::input_buffer rk{range.data(), range.data() + range.size()};
            kv::key_to_native<uint8_t>(rk);
            kv::read_index_prefix(rk, table_name, index_name);
            auto& table = get_kv_table(table_name);
            auto& index = *table.trim_index_obj;

            uint32_t prev_block = 0xffff'ffff;
            rdb::for_each(rocksdb_inst->database, range, range, [&](auto k, auto) {
                uint32_t          block;
                bool              present_k;
                auto              suffix_pos = kv::extract_from_index(k, table, index.sort_keys, block, present_k);
                std::vector<char> partial_k{k.pos, suffix_pos};

                if (prev_block <= end_trim) {
                    auto pk = extract_pk(k, table, block, present_k);
                    remove_row(batch, batch, {pk.data(), pk.data() + pk.size()}, &num_rows, &num_indexes);
                }
                prev_block = block;
                return true;
            });
        }

        ilog("trim: removed ${r} rows and ${d} index entries", ("r", num_rows)("d", num_indexes));
        first = end_trim;
        write_fill_status(batch);
        write(rocksdb_inst->database, batch);
    }

    const abi_type& get_type(const std::string& name) { return connection->get_type(name); }

    void closed(bool retry) override {
        if (my) {
            my->session.reset();
            if (retry)
                my->schedule_retry();
        }
    }

    ~flm_session() {}
}; // flm_session

static abstract_plugin& _fill_rocksdb_plugin = app().register_plugin<fill_rocksdb_plugin>();

fill_rocksdb_plugin_impl::~fill_rocksdb_plugin_impl() {
    if (session)
        session->my = nullptr;
}

void fill_rocksdb_plugin_impl::start() {
    session = std::make_shared<flm_session>(this);
    session->connect(app().get_io_service());
}

fill_rocksdb_plugin::fill_rocksdb_plugin()
    : my(std::make_shared<fill_rocksdb_plugin_impl>()) {}

fill_rocksdb_plugin::~fill_rocksdb_plugin() {}

void fill_rocksdb_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto clop = cli.add_options();
    clop("frdb-check", "Check database");
}

void fill_rocksdb_plugin::plugin_initialize(const variables_map& options) {
    try {
        auto endpoint = options.at("fill-connect-to").as<std::string>();
        if (endpoint.find(':') == std::string::npos)
            throw std::runtime_error("invalid endpoint: " + endpoint);

        auto port                = endpoint.substr(endpoint.find(':') + 1, endpoint.size());
        auto host                = endpoint.substr(0, endpoint.find(':'));
        my->config->host         = host;
        my->config->port         = port;
        my->config->skip_to      = options.count("fill-skip-to") ? options["fill-skip-to"].as<uint32_t>() : 0;
        my->config->stop_before  = options.count("fill-stop") ? options["fill-stop"].as<uint32_t>() : 0;
        my->config->trx_filters  = fill_plugin::get_trx_filters(options);
        my->config->enable_trim  = options.count("fill-trim");
        my->config->enable_check = options.count("frdb-check");
    }
    FC_LOG_AND_RETHROW()
}

void fill_rocksdb_plugin::plugin_startup() { my->start(); }

void fill_rocksdb_plugin::plugin_shutdown() {
    if (my->session)
        my->session->connection->close(false);
    my->timer.cancel();
    ilog("fill_rocksdb_plugin stopped");
}
