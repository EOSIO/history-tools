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
    abieos::input_buffer           pos         = {}; // temporary filled by fill()
};

struct rocksdb_index {
    abieos::name                name   = {};
    std::vector<rocksdb_field*> fields = {};
};

struct rocksdb_table {
    std::string                                 name        = {};
    abieos::name                                short_name  = {};
    const abieos::abi_type*                     abi_type    = {};
    std::vector<std::unique_ptr<rocksdb_field>> fields      = {};
    std::map<std::string, rocksdb_field*>       field_map   = {};
    std::unique_ptr<rocksdb_index>              delta_index = {};
    std::map<abieos::name, rocksdb_index>       indexes     = {};
};

struct fill_rocksdb_config : connection_config {
    uint32_t skip_to      = 0;
    uint32_t stop_before  = 0;
    bool     enable_trim  = false;
    bool     enable_check = false;
};

struct fill_rocksdb_plugin_impl : std::enable_shared_from_this<fill_rocksdb_plugin_impl> {
    std::shared_ptr<fill_rocksdb_config> config = std::make_shared<fill_rocksdb_config>();
    std::shared_ptr<::flm_session>       session;

    ~fill_rocksdb_plugin_impl();
};

struct flm_session : connection_callbacks, std::enable_shared_from_this<flm_session> {
    fill_rocksdb_plugin_impl*                  my = nullptr;
    std::shared_ptr<fill_rocksdb_config>       config;
    std::shared_ptr<::rocksdb_inst>            rocksdb_inst = app().find_plugin<rocksdb_plugin>()->get_rocksdb_inst();
    std::optional<rocksdb::WriteBatch>         active_batch;
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
        // for_each_subkey(rocksdb_inst->database, kv::make_block_key(0), kv::make_block_key(0xffff'ffff), [&](auto&, auto k, auto v) {
        //     auto orig_k = k;
        //     if (kv::bin_to_key_tag(k) != kv::key_tag::block)
        //         throw std::runtime_error("This shouldn't happen (1)");
        //     auto block_num = kv::bin_to_native_key<uint32_t>(k);
        //     auto tag       = kv::bin_to_key_tag(k);
        //     if (tag == kv::key_tag::table_row && (block_num < first || block_num > head))
        //         throw std::runtime_error(
        //             "Saw row for block_num " + std::to_string(block_num) +
        //             ", which is out of range [first, head]. key: " + kv::key_to_string(orig_k));
        //     if (tag != kv::key_tag::received_block)
        //         return true;
        //     if (block_num == first || block_num == head || !(block_num % 10'000))
        //         ilog("Found records for block ${b}", ("b", block_num));
        //     if (block_num != expected)
        //         throw std::runtime_error(
        //             "Saw received_block record " + std::to_string(block_num) + " but expected " + std::to_string(expected));
        //     ++expected;
        //     return true;
        // });
        ilog("Found records for block ${b}", ("b", expected - 1));
        if (expected - 1 != head)
            throw std::runtime_error("Found head " + std::to_string(expected - 1) + " but fill_status.head = " + std::to_string(head));

        ilog("verifying table_index keys reference existing records");
        uint32_t num_ti_keys = 0;
        for_each(rocksdb_inst->database, kv::make_index_key(), kv::make_index_key(), [&](auto k, auto v) {
            if (!((++num_ti_keys) % 1'000'000))
                ilog("Checked ${n} table_index keys", ("n", num_ti_keys));
            if (!rdb::exists(rocksdb_inst->database, rdb::to_slice(v)))
                throw std::runtime_error("A table_index key references a missing record");
            return true;
        });
        ilog("Checked ${n} table_index keys", ("n", num_ti_keys));
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

    void add_table(const std::string& table_name, const std::string& table_type, const jarray& key_names) {
        auto table_name_it = kv::table_names.find(table_name);
        if (table_name_it == kv::table_names.end())
            throw std::runtime_error("unknown table \"" + table_name + "\"");
        if (tables.find(table_name) != tables.end())
            throw std::runtime_error("duplicate table \"" + table_name + "\"");
        auto& table      = tables[table_name];
        table.name       = table_name;
        table.short_name = table_name_it->second;
        table.abi_type   = &get_type(table_type);

        if (!table.abi_type->filled_variant || table.abi_type->fields.size() != 1 || !table.abi_type->fields[0].type->filled_struct)
            throw std::runtime_error("don't know how to process " + table.abi_type->name);

        for (auto& f : table.abi_type->fields[0].type->fields)
            fill_fields(table, "", f);

        table.delta_index = std::make_unique<rocksdb_index>();
        auto& trim_index  = table.indexes["trim"_n];
        trim_index.name   = "trim"_n;
        for (auto& key : key_names) {
            auto& field_name = std::get<std::string>(key.value);
            auto  it         = table.field_map.find(field_name);
            if (it == table.field_map.end())
                throw std::runtime_error("table \"" + table_name + "\" key \"" + field_name + "\" not found");
            table.delta_index->fields.push_back(it->second);
            trim_index.fields.push_back(it->second);
        }
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

        block_info_table             = &tables["block_info"];
        block_info_table->name       = "block_info";
        block_info_table->short_name = "block.info"_n;
        fill_fields(*block_info_table, "", abieos::abi_field{"block_num", &get_type("uint32")});
        fill_fields(*block_info_table, "", abieos::abi_field{"block_id", &get_type("checksum256")});
        fill_fields(*block_info_table, "", abieos::abi_field{"timestamp", &get_type("block_timestamp_type")});
        fill_fields(*block_info_table, "", abieos::abi_field{"producer", &get_type("name")});
        fill_fields(*block_info_table, "", abieos::abi_field{"confirmed", &get_type("uint16")});
        fill_fields(*block_info_table, "", abieos::abi_field{"previous", &get_type("checksum256")});
        fill_fields(*block_info_table, "", abieos::abi_field{"transaction_mroot", &get_type("checksum256")});
        fill_fields(*block_info_table, "", abieos::abi_field{"action_mroot", &get_type("checksum256")});
        fill_fields(*block_info_table, "", abieos::abi_field{"schedule_version", &get_type("uint32")});
        auto& block_info_trim = block_info_table->indexes["trim"_n];
        block_info_trim.name  = "trim"_n;
        block_info_trim.fields.push_back(block_info_table->field_map["block_num"]);

        action_trace_table             = &tables["action_trace"];
        action_trace_table->name       = "action_trace";
        action_trace_table->short_name = "atrace"_n;
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
        auto& action_trace_trim = action_trace_table->indexes["trim"_n];
        action_trace_trim.name  = "trim"_n;
        action_trace_trim.fields.push_back(action_trace_table->field_map["block_num"]);
        action_trace_trim.fields.push_back(action_trace_table->field_map["transaction_id"]);
        action_trace_trim.fields.push_back(action_trace_table->field_map["action_ordinal"]);
    } // init_tables

    void init_indexes() {
        // todo: verify index set in config matches index set in db
        auto& c = rocksdb_inst->query_config;
        for (auto& [query_name, query] : c.query_map) {
            auto table_it = tables.find(query->table);
            if (table_it == tables.end())
                throw std::runtime_error("can't find table " + query->table);
            auto& table = table_it->second;

            auto index_it = table.indexes.find(query_name);
            if (index_it != table.indexes.end())
                throw std::runtime_error("duplicate index " + query->table + "." + (std::string)query_name);
            auto& index = table.indexes[query_name];
            index.name  = query_name;

            for (auto& key : query->sort_keys) {
                auto field_it = table.field_map.find(key.name);
                if (field_it == table.field_map.end())
                    throw std::runtime_error("can't find " + query->table + "." + key.name);
                index.fields.push_back(field_it->second);
            }
        }
    }

    void received_abi(std::string_view abi) override {
        init_tables(abi);
        init_indexes();

        load_fill_status();
        rocksdb::WriteBatch batch;
        truncate(batch, head + 1);
        write(rocksdb_inst->database, batch);

        connection->send(get_status_request_v0{});
    }

    bool received(get_status_result_v0& status) override {
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
                auto rb = rdb::get<kv::received_block>(rocksdb_inst->database, kv::make_received_block_key(i));
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

    void truncate(rocksdb::WriteBatch& batch, uint32_t block) {
        // for_each(rocksdb_inst->database, kv::make_block_key(block), kv::make_block_key(), [&](auto k, auto v) {
        //     rdb::check(batch.Delete(rdb::to_slice(k)), "truncate (1): ");
        //     return true;
        // });
        // for_each(rocksdb_inst->database, kv::make_table_index_ref_key(block), kv::make_table_index_ref_key(), [&](auto k, auto v) {
        //     std::vector<char> k2{k.pos, k.end};
        //     rdb::check(batch.Delete(rdb::to_slice(v)), "truncate (2): ");
        //     rdb::check(batch.Delete(rdb::to_slice(k2)), "truncate (3): ");
        //     return true;
        // });

        auto rb = rdb::get<kv::received_block>(rocksdb_inst->database, kv::make_received_block_key(block - 1), false);
        if (!rb) {
            head    = 0;
            head_id = {};
        } else {
            head    = block - 1;
            head_id = rb->block_id;
        }
        first = std::min(first, head);
    }

    bool received(get_blocks_result_v0& result) override {
        if (!result.this_block)
            return true;
        if (config->stop_before && result.this_block->block_num >= config->stop_before) {
            ilog("block ${b}: stop requested", ("b", result.this_block->block_num));
            write(rocksdb_inst->database, *active_batch);
            active_batch.reset();
            return false;
        }

        if (!active_batch)
            active_batch.emplace();
        try {
            if (result.this_block->block_num <= head) {
                ilog("switch forks at block ${b}", ("b", result.this_block->block_num));
                truncate(*active_batch, result.this_block->block_num);
                // !!! todo: fill status is out of sync because of early batch writes
            }

            bool near       = result.this_block->block_num + 4 >= result.last_irreversible.block_num;
            bool commit_now = !(result.this_block->block_num % 200) || near;
            if (commit_now)
                ilog("block ${b}", ("b", result.this_block->block_num));

            if (head_id != abieos::checksum256{} && (!result.prev_block || result.prev_block->block_id != head_id))
                throw std::runtime_error("prev_block does not match");
            if (result.block)
                receive_block(result.this_block->block_num, result.this_block->block_id, *result.block, *active_batch);
            if (result.deltas)
                receive_deltas(*active_batch, result.this_block->block_num, *result.deltas);
            if (result.traces)
                receive_traces(*active_batch, result.this_block->block_num, *result.traces);

            head            = result.this_block->block_num;
            head_id         = result.this_block->block_id;
            irreversible    = result.last_irreversible.block_num;
            irreversible_id = result.last_irreversible.block_id;
            if (!first)
                first = head;
            write_fill_status(*active_batch);

            rdb::put(
                *active_batch, kv::make_received_block_key(result.this_block->block_num),
                kv::received_block{result.this_block->block_num, result.this_block->block_id});

            if (commit_now) {
                write(rocksdb_inst->database, *active_batch);
                if (config->enable_trim) {
                    trim(*active_batch);
                    write_fill_status(*active_batch);
                    write(rocksdb_inst->database, *active_batch);
                }
                active_batch.reset();
            }
            if (near)
                rocksdb_inst->database.flush(false, false);
        } catch (...) {
            active_batch.reset();
            throw;
        }

        return true;
    } // receive_result()

    void fill(std::vector<char>& dest, input_buffer& src, rocksdb_field& field) {
        field.pos = src;
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

    void fill_key(std::vector<char>& dest, rocksdb_index& index) {
        for (auto& field : index.fields) {
            auto pos = field->pos;
            field->type->bin_to_key(dest, pos);
        }
    }

    void receive_block(uint32_t block_num, const checksum256& block_id, input_buffer bin, rocksdb::WriteBatch& batch) {
        state_history::signed_block block;
        bin_to_native(block, bin);
        auto              key = kv::make_block_info_key(block_num);
        std::vector<char> value;

        std::vector<uint32_t> positions;
        auto                  f = [&](const auto& x) {
            positions.push_back(value.size());
            abieos::native_to_bin(x, value);
        };

        f(block_num);
        f(block_id);
        f(block.timestamp);
        f(block.producer);
        f(block.confirmed);
        f(block.previous);
        f(block.transaction_mroot);
        f(block.action_mroot);
        f(block.schedule_version);
        abieos::native_to_bin(block.new_producers ? *block.new_producers : state_history::producer_schedule{}, value);

        rdb::put(batch, key, value);
        for (size_t i = 0; i < positions.size(); ++i)
            block_info_table->fields[i]->pos = {value.data() + positions[i], value.data() + value.size()};

        std::vector<char> index_key;
        for (auto& [_, index] : block_info_table->indexes) {
            index_key.clear();
            kv::append_index_key(index_key, block_info_table->short_name, index.name);
            fill_key(index_key, index);
            kv::append_index_suffix(index_key, block_num, true);
            batch.Put(rdb::to_slice(index_key), {});
        }
    } // receive_block

    void receive_deltas(rocksdb::WriteBatch& batch, uint32_t block_num, input_buffer bin) {
        auto&             table_delta_type = get_type("table_delta");
        std::vector<char> delta_key;
        std::vector<char> value;
        std::vector<char> index_key;

        auto num = read_varuint32(bin);
        for (uint32_t i = 0; i < num; ++i) {
            check_variant(bin, table_delta_type, "table_delta_v0");
            state_history::table_delta_v0 table_delta;
            bin_to_native(table_delta, bin);

            auto table_it = tables.find(table_delta.name);
            if (table_it == tables.end())
                throw std::runtime_error("unknown table \"" + table_delta.name + "\"");
            auto& table = table_it->second;

            size_t num_processed = 0;
            for (auto& row : table_delta.rows) {
                if (table_delta.rows.size() > 10000 && !(num_processed % 10000)) {
                    ilog(
                        "block ${b} ${t} ${n} of ${r}",
                        ("b", block_num)("t", table_delta.name)("n", num_processed)("r", table_delta.rows.size()));
                    write(rocksdb_inst->database, batch);
                }
                check_variant(row.data, *table.abi_type, 0u);
                delta_key.clear();
                kv::append_table_key(delta_key, block_num, row.present, table.short_name);
                value.clear();
                abieos::native_to_bin(block_num, value);
                abieos::native_to_bin(row.present, value);
                for (auto& field : table.fields)
                    fill(value, row.data, *field);
                fill_key(delta_key, *table.delta_index);
                rdb::put(batch, delta_key, value);

                for (auto& [_, index] : table.indexes) {
                    index_key.clear();
                    kv::append_index_key(index_key, table.short_name, index.name);
                    fill_key(index_key, index);
                    kv::append_index_suffix(index_key, block_num, row.present);
                    batch.Put(rdb::to_slice(index_key), {});
                }
                ++num_processed;
            }
        }
    } // receive_deltas

    void receive_traces(rocksdb::WriteBatch& batch, uint32_t block_num, input_buffer bin) {
        auto     num          = read_varuint32(bin);
        uint32_t num_ordinals = 0;
        for (uint32_t i = 0; i < num; ++i) {
            state_history::transaction_trace trace;
            bin_to_native(trace, bin);
            write_transaction_trace(batch, block_num, num_ordinals, std::get<state_history::transaction_trace_v0>(trace));
        }
    }

    void write_transaction_trace(
        rocksdb::WriteBatch& batch, uint32_t block_num, uint32_t& num_ordinals, const state_history::transaction_trace_v0& ttrace) {
        auto* failed = !ttrace.failed_dtrx_trace.empty()
                           ? &std::get<state_history::transaction_trace_v0>(ttrace.failed_dtrx_trace[0].recurse)
                           : nullptr;
        if (failed)
            write_transaction_trace(batch, block_num, num_ordinals, *failed);
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

        std::vector<char> index_key;
        for (auto& atrace : ttrace.action_traces)
            write_action_trace(batch, block_num, ttrace, std::get<state_history::action_trace_v0>(atrace), key, value, index_key);
    }

    void write_action_trace(
        rocksdb::WriteBatch& batch, uint32_t block_num, const state_history::transaction_trace_v0& ttrace,
        const state_history::action_trace_v0& atrace, std::vector<char>& key, std::vector<char>& value, std::vector<char>& index_key) {
        key.clear();
        kv::append_action_trace_key(key, block_num, ttrace.id, atrace.action_ordinal.value);
        value.clear();

        std::vector<uint32_t> positions;
        auto                  f = [&](const auto& x) {
            positions.push_back(value.size());
            abieos::native_to_bin(x, value);
        };

        f(block_num);
        f(ttrace.id);
        f((uint8_t)ttrace.status);
        f(atrace.action_ordinal);
        f(atrace.creator_action_ordinal);
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
        f(atrace.receiver);
        f(atrace.act.account);
        f(atrace.act.name);
        f(atrace.act.data);
        f(atrace.context_free);
        f(atrace.elapsed);
        f(atrace.console);
        f(atrace.except ? *atrace.except : "");
        f(atrace.error_code ? *atrace.error_code : 0);

        abieos::native_to_bin(atrace.console, value);
        abieos::native_to_bin(atrace.except ? *atrace.except : std::string(), value);
        rdb::put(batch, key, value);

        for (size_t i = 0; i < positions.size(); ++i)
            action_trace_table->fields[i]->pos = {value.data() + positions[i], value.data() + value.size()};

        for (auto& [_, index] : action_trace_table->indexes) {
            index_key.clear();
            kv::append_index_key(index_key, action_trace_table->short_name, index.name);
            fill_key(index_key, index);
            kv::append_index_suffix(index_key, block_num, true);
            rdb::put(batch, index_key, key);
        }

        // todo: receipt_auth_sequence
        // todo: authorization
        // todo: account_ram_deltas
    }

    template <typename F>
    void for_each_row_in_block(uint32_t block_num, F f) {
        auto row_bound = kv::make_table_key(block_num);
        rdb::for_each(rocksdb_inst->database, row_bound, row_bound, [&](auto k, auto row_content) {
            k.pos += row_bound.size();
            auto table_name = kv::bin_to_native_key<abieos::name>(k);
            auto present_k  = kv::bin_to_native_key<bool>(k);
            auto pk         = k;
            return f(table_name, present_k, pk);
        });
    }

    template <typename F>
    void for_each_delta_in_block(uint32_t block_num, F f) {
        std::vector<char> delta_bound;
        kv::append_table_key(delta_bound, block_num);
        rdb::for_each(rocksdb_inst->database, delta_bound, delta_bound, [&](auto k, auto row_content) {
            k.pos += delta_bound.size();
            auto table_name = kv::bin_to_native_key<abieos::name>(k);
            auto present    = kv::bin_to_native_key<bool>(k);
            auto pk         = k;
            return f(table_name, present, pk);
        });
    }

    template <typename F>
    void for_each_row_trim(abieos::name table_name, abieos::input_buffer pk, uint32_t block_num, F f) {
        std::vector<char> trim_bound;
        kv::append_index_key(trim_bound, table_name, "trim"_n);
        kv::native_to_bin_key<uint32_t>(trim_bound, block_num);
        rdb::for_each(rocksdb_inst->database, trim_bound, trim_bound, [&](auto k, auto v) { return f(v); });
    }

    template <typename F>
    void for_each_delta_trim(abieos::name table_name, abieos::input_buffer pk, uint32_t max_block_num, F f) {
        std::vector<char> trim_bound;
        kv::append_index_key(trim_bound, table_name, "trim"_n);
        trim_bound.insert(trim_bound.end(), pk.pos, pk.end);
        auto trim_lower_bound = trim_bound;
        kv::native_to_bin_key<uint32_t>(trim_lower_bound, ~max_block_num);

        rdb::for_each(rocksdb_inst->database, trim_lower_bound, trim_bound, [&](auto k, auto v) {
            k.pos += trim_bound.size();
            auto block_num = ~kv::bin_to_native_key<uint32_t>(k);
            auto present   = !kv::bin_to_native_key<bool>(k);
            return f(block_num, present);
        });
    }

    void remove_row(rocksdb::WriteBatch& batch, abieos::name table_name, uint32_t block_num, std::vector<char> key) {
        // auto index_ref_bounds = kv::make_table_index_ref_key(block_num, key);
        // rdb::for_each(rocksdb_inst->database, index_ref_bounds, index_ref_bounds, [&](auto k, auto v) {
        //     rdb::check(batch.Delete(rdb::to_slice(v)), "remove_row (1): ");
        //     rdb::check(batch.Delete(rdb::to_slice(k)), "remove_row (2): ");
        //     return true;
        // });
        // rdb::check(batch.Delete(rdb::to_slice(key)), "remove_row (3): ");
    }

    void remove_delta(rocksdb::WriteBatch& batch, abieos::name table_name, uint32_t block_num, bool present, abieos::input_buffer pk) {
        std::vector<char> delta_key;
        kv::append_table_key(delta_key, block_num, present, table_name);
        delta_key.insert(delta_key.end(), pk.pos, pk.end);
        remove_row(batch, table_name, block_num, delta_key);
    }

    void trim(rocksdb::WriteBatch& batch) {
        // todo: optimize batching
        if (!config->enable_trim)
            return;
        auto end_trim = std::min(head, irreversible);
        if (first >= end_trim)
            return;
        ilog("trim: ${b} - ${e}", ("b", first)("e", end_trim));
        rocksdb_inst->database.flush(true, true);

        uint64_t num_rows_removed   = 0;
        uint64_t num_deltas_removed = 0;
        for (auto block_num = first; block_num < end_trim; ++block_num) {
            if (end_trim - first >= 400 && !(block_num % 100)) {
                ilog("trim: removed ${r} rows and ${d} deltas so far", ("r", num_rows_removed)("d", num_deltas_removed));
                ilog("trim ${x}", ("x", block_num));
            }
            // for_each_row_in_block(block_num, [&](auto row_table_name, auto row_pk) {
            //     for_each_row_trim(row_table_name, row_pk, block_num, [&](auto table_key) {
            //         remove_row(batch, row_table_name, block_num, {table_key.pos, table_key.end});
            //         ++num_rows_removed;
            //         return true;
            //     });
            //     return true;
            // });
            for_each_delta_in_block(block_num + 1, [&](auto delta_table_name, auto delta_present, auto delta_pk) {
                for_each_delta_trim(delta_table_name, delta_pk, block_num + 1, [&](auto trim_block_num, auto trim_present) {
                    if (trim_block_num == block_num + 1)
                        return true;
                    if (trim_block_num > block_num + 1)
                        throw std::runtime_error("found unexpected block in trim search: " + std::to_string(trim_block_num));
                    remove_delta(batch, delta_table_name, trim_block_num, trim_present, delta_pk);
                    ++num_deltas_removed;
                    return true;
                });
                return true;
            });
            rdb::check(batch.Delete(rdb::to_slice(kv::make_received_block_key(block_num))), "trim: ");
        }

        ilog("trim: removed ${r} rows and ${d} deltas", ("r", num_rows_removed)("d", num_deltas_removed));
        first = end_trim;
    }

    const abi_type& get_type(const std::string& name) { return connection->get_type(name); }

    void closed() override {
        if (my)
            my->session.reset();
    }

    ~flm_session() { ilog("fill_rocksdb_plugin stopped"); }
}; // flm_session

static abstract_plugin& _fill_rocksdb_plugin = app().register_plugin<fill_rocksdb_plugin>();

fill_rocksdb_plugin_impl::~fill_rocksdb_plugin_impl() {
    if (session)
        session->my = nullptr;
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
        my->config->enable_trim  = options.count("fill-trim");
        my->config->enable_check = options.count("frdb-check");
    }
    FC_LOG_AND_RETHROW()
}

void fill_rocksdb_plugin::plugin_startup() {
    my->session = std::make_shared<flm_session>(my.get());
    if (my->config->enable_check)
        my->session->check();
    my->session->connect(app().get_io_service());
}

void fill_rocksdb_plugin::plugin_shutdown() {
    if (my->session)
        my->session->connection->close();
}
