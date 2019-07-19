// copyright defined in LICENSE.txt

#include "fill_lmdb_plugin.hpp"
#include "state_history_connection.hpp"
#include "state_history_lmdb.hpp"
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
namespace lmdb      = state_history::lmdb;
namespace websocket = boost::beast::websocket;

using asio::ip::tcp;
using boost::beast::flat_buffer;
using boost::system::error_code;

struct flm_session;

struct lmdb_table;

struct lmdb_field {
    std::string                 name        = {};
    const abieos::abi_field*    abi_field   = {};
    const kv::type*             type        = {};
    std::unique_ptr<lmdb_table> array_of    = {};
    std::unique_ptr<lmdb_table> optional_of = {};
    abieos::input_buffer        pos         = {}; // temporary filled by fill()
};

struct lmdb_index {
    abieos::name             name   = {};
    std::vector<lmdb_field*> fields = {};
};

struct lmdb_table {
    std::string                              name        = {};
    abieos::name                             short_name  = {};
    const abieos::abi_type*                  abi_type    = {};
    std::vector<std::unique_ptr<lmdb_field>> fields      = {};
    std::map<std::string, lmdb_field*>       field_map   = {};
    std::unique_ptr<lmdb_index>              delta_index = {};
    std::map<abieos::name, lmdb_index>       indexes     = {};
};

struct fill_lmdb_config : connection_config {
    uint32_t skip_to      = 0;
    uint32_t stop_before  = 0;
    bool     enable_trim  = false;
    bool     enable_check = false;
};

struct fill_lmdb_plugin_impl : std::enable_shared_from_this<fill_lmdb_plugin_impl> {
    std::shared_ptr<fill_lmdb_config> config = std::make_shared<fill_lmdb_config>();
    std::shared_ptr<::flm_session>    session;

    ~fill_lmdb_plugin_impl();
};

struct flm_session : connection_callbacks, std::enable_shared_from_this<flm_session> {
    fill_lmdb_plugin_impl*                     my = nullptr;
    std::shared_ptr<fill_lmdb_config>          config;
    std::shared_ptr<::lmdb_inst>               lmdb_inst = app().find_plugin<lmdb_plugin>()->get_lmdb_inst();
    std::optional<lmdb::transaction>           active_tx;
    std::shared_ptr<state_history::connection> connection;
    std::map<std::string, lmdb_table>          tables             = {};
    lmdb_table*                                block_info_table   = {};
    lmdb_table*                                action_trace_table = {};
    std::optional<state_history::fill_status>  current_db_status  = {};
    uint32_t                                   head               = 0;
    abieos::checksum256                        head_id            = {};
    uint32_t                                   irreversible       = 0;
    abieos::checksum256                        irreversible_id    = {};
    uint32_t                                   first              = 0;

    flm_session(fill_lmdb_plugin_impl* my)
        : my(my)
        , config(my->config) {}

    void connect(asio::io_context& ioc) {
        connection = std::make_shared<state_history::connection>(ioc, *config, shared_from_this());
        connection->connect();
    }

    void check() {
        ilog("checking database");
        lmdb::transaction t{lmdb_inst->lmdb_env, false};
        load_fill_status(t);
        if (!current_db_status) {
            ilog("database is empty");
            return;
        }
        ilog("first: ${first}, irreversible: ${irreversible}, head: ${head}", ("first", first)("irreversible", irreversible)("head", head));

        ilog("verifying expected records are present");
        uint32_t expected = first;
        for_each_subkey(t, lmdb_inst->db, kv::make_block_key(0), kv::make_block_key(0xffff'ffff), [&](auto&, auto k, auto v) {
            auto orig_k = k;
            if (kv::bin_to_key_tag(k) != kv::key_tag::block)
                throw std::runtime_error("This shouldn't happen (1)");
            auto block_num = kv::bin_to_native_key<uint32_t>(k);
            auto tag       = kv::bin_to_key_tag(k);
            if (tag == kv::key_tag::table_row && (block_num < first || block_num > head))
                throw std::runtime_error(
                    "Saw row for block_num " + std::to_string(block_num) +
                    ", which is out of range [first, head]. key: " + kv::key_to_string(orig_k));
            if (tag != kv::key_tag::received_block)
                return true;
            if (block_num == first || block_num == head || !(block_num % 10'000))
                ilog("Found records for block ${b}", ("b", block_num));
            if (block_num != expected)
                throw std::runtime_error(
                    "Saw received_block record " + std::to_string(block_num) + " but expected " + std::to_string(expected));
            ++expected;
            return true;
        });
        ilog("Found records for block ${b}", ("b", expected - 1));
        if (expected - 1 != head)
            throw std::runtime_error("Found head " + std::to_string(expected - 1) + " but fill_status.head = " + std::to_string(head));

        ilog("verifying table_index keys reference existing records");
        uint32_t num_ti_keys = 0;
        for_each(t, lmdb_inst->db, kv::make_table_index_key(), kv::make_table_index_key(), [&](auto k, auto v) {
            if (!((++num_ti_keys) % 1'000'000))
                ilog("Checked ${n} table_index keys", ("n", num_ti_keys));
            if (!lmdb::exists(t, lmdb_inst->db, lmdb::to_const_val(v)))
                throw std::runtime_error("A table_index key references a missing record");
            return true;
        });
        ilog("Checked ${n} table_index keys", ("n", num_ti_keys));

        ilog("verifying table_index_ref keys reference existing table_index records");
        uint32_t num_ti_ref_keys = 0;
        for_each(t, lmdb_inst->db, kv::make_table_index_ref_key(), kv::make_table_index_ref_key(), [&](auto k, auto v) {
            if (!((++num_ti_ref_keys) % 1'000'000))
                ilog("Checked ${n} table_index_ref keys", ("n", num_ti_ref_keys));
            if (!lmdb::exists(t, lmdb_inst->db, lmdb::to_const_val(v)))
                throw std::runtime_error("A table_index_ref key references a missing table_index record");
            return true;
        });
        ilog("Checked ${n} table_index_ref keys", ("n", num_ti_ref_keys));
    }

    void fill_fields(lmdb_table& table, const std::string& base_name, const abieos::abi_field& abi_field) {
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
                throw std::runtime_error("don't know lmdb type for abi type: " + raw_type->name);

            table.fields.push_back(std::make_unique<lmdb_field>());
            auto* f                     = table.fields.back().get();
            table.field_map[field_name] = f;
            f->name                     = field_name;
            f->abi_field                = &abi_field;
            f->type                     = (array_of_struct | array_of_variant | optional_of_struct) ? nullptr : &type_it->second;

            if (array_of_struct) {
                f->array_of = std::make_unique<lmdb_table>(lmdb_table{.name = field_name, .abi_type = abi_field.type->array_of});
                for (auto& g : abi_field.type->array_of->fields)
                    fill_fields(*f->array_of, base_name, g);
            } else if (array_of_variant) {
                f->array_of =
                    std::make_unique<lmdb_table>(lmdb_table{.name = field_name, .abi_type = abi_field.type->array_of->fields[0].type});
                for (auto& g : abi_field.type->array_of->fields[0].type->fields)
                    fill_fields(*f->array_of, base_name, g);
            } else if (optional_of_struct) {
                f->optional_of = std::make_unique<lmdb_table>(lmdb_table{.name = field_name, .abi_type = abi_field.type->optional_of});
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

        table.delta_index = std::make_unique<lmdb_index>();
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
        auto& c = lmdb_inst->query_config;
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
        connection->send(get_status_request_v0{});
    }

    bool received(get_status_result_v0& status) override {
        lmdb::transaction t{lmdb_inst->lmdb_env, true};
        load_fill_status(t);
        auto positions = get_positions(t);
        truncate(t, head + 1);
        t.commit();

        connection->request_blocks(status, std::max(config->skip_to, head + 1), positions);
        return true;
    }

    void load_fill_status(lmdb::transaction& t) {
        current_db_status = lmdb::get<state_history::fill_status>(t, lmdb_inst->db, kv::make_fill_status_key(), false);
        if (!current_db_status)
            return;
        head            = current_db_status->head;
        head_id         = current_db_status->head_id;
        irreversible    = current_db_status->irreversible;
        irreversible_id = current_db_status->irreversible_id;
        first           = current_db_status->first;
    }

    void check_conflicts(lmdb::transaction& t) {
        auto r = lmdb::get<state_history::fill_status>(t, lmdb_inst->db, kv::make_fill_status_key(), false);
        if ((bool)r != (bool)current_db_status || (r && *r != *current_db_status))
            throw std::runtime_error("Another process is filling this database");
    }

    std::vector<block_position> get_positions(lmdb::transaction& t) {
        std::vector<block_position> result;
        if (head) {
            for (uint32_t i = irreversible; i <= head; ++i) {
                auto rb = lmdb::get<kv::received_block>(t, lmdb_inst->db, kv::make_received_block_key(i));
                result.push_back({rb->block_num, rb->block_id});
            }
        }
        return result;
    }

    void write_fill_status(lmdb::transaction& t) {
        if (irreversible < head)
            current_db_status = state_history::fill_status{
                .head = head, .head_id = head_id, .irreversible = irreversible, .irreversible_id = irreversible_id, .first = first};
        else
            current_db_status = state_history::fill_status{
                .head = head, .head_id = head_id, .irreversible = head, .irreversible_id = head_id, .first = first};
        put(t, lmdb_inst->db, kv::make_fill_status_key(), *current_db_status, true);
    }

    void truncate(lmdb::transaction& t, uint32_t block) {
        for_each(t, lmdb_inst->db, kv::make_block_key(block), kv::make_block_key(), [&](auto k, auto v) {
            lmdb::check(mdb_del(t.tx, lmdb_inst->db.db, lmdb::addr(lmdb::to_const_val(k)), nullptr), "truncate (1): ");
            return true;
        });
        for_each(t, lmdb_inst->db, kv::make_table_index_ref_key(block), kv::make_table_index_ref_key(), [&](auto k, auto v) {
            std::vector<char> k2{k.pos, k.end};
            lmdb::check(mdb_del(t.tx, lmdb_inst->db.db, lmdb::addr(lmdb::to_const_val(v)), nullptr), "truncate (2): ");
            lmdb::check(mdb_del(t.tx, lmdb_inst->db.db, lmdb::addr(lmdb::to_const_val(k2)), nullptr), "truncate (3): ");
            return true;
        });

        auto rb = lmdb::get<kv::received_block>(t, lmdb_inst->db, kv::make_received_block_key(block - 1), false);
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
            active_tx->commit();
            active_tx.reset();
            return false;
        }

        if (!active_tx)
            active_tx.emplace(lmdb_inst->lmdb_env, true);
        try {
            check_conflicts(*active_tx);
            if (result.this_block->block_num <= head) {
                ilog("switch forks at block ${b}", ("b", result.this_block->block_num));
                truncate(*active_tx, result.this_block->block_num);
            }

            bool commit_now =
                !(result.this_block->block_num % 200) || result.this_block->block_num + 4 >= result.last_irreversible.block_num;
            if (commit_now)
                ilog("block ${b}", ("b", result.this_block->block_num));

            if (head_id != abieos::checksum256{} && (!result.prev_block || result.prev_block->block_id != head_id))
                throw std::runtime_error("prev_block does not match");
            if (result.block)
                receive_block(result.this_block->block_num, result.this_block->block_id, *result.block, *active_tx);
            if (result.deltas)
                receive_deltas(*active_tx, result.this_block->block_num, *result.deltas);
            if (result.traces)
                receive_traces(*active_tx, result.this_block->block_num, *result.traces);

            head            = result.this_block->block_num;
            head_id         = result.this_block->block_id;
            irreversible    = result.last_irreversible.block_num;
            irreversible_id = result.last_irreversible.block_id;
            if (!first)
                first = head;
            write_fill_status(*active_tx);

            put(*active_tx, lmdb_inst->db, kv::make_received_block_key(result.this_block->block_num),
                kv::received_block{result.this_block->block_num, result.this_block->block_id});

            if (commit_now) {
                if (config->enable_trim) {
                    trim(*active_tx);
                    write_fill_status(*active_tx);
                }
                active_tx->commit();
                active_tx.reset();
            }
        } catch (...) {
            active_tx.reset();
            throw;
        }

        return true;
    } // receive_result()

    void fill(std::vector<char>& dest, input_buffer& src, lmdb_field& field) {
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

    void fill_key(std::vector<char>& dest, lmdb_index& index) {
        for (auto& field : index.fields) {
            auto pos = field->pos;
            field->type->bin_to_bin_key(dest, pos);
        }
    }

    void receive_block(uint32_t block_num, const checksum256& block_id, input_buffer bin, lmdb::transaction& t) {
        state_history::signed_block block;
        bin_to_native(block, bin);
        auto              key = kv::make_block_info_key(block_num);
        std::vector<char> value;

        std::vector<uint32_t> positions;
        auto                  f = [&](const auto& x) {
            positions.push_back(value.size());
            abieos::native_to_bin(value, x);
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
        abieos::native_to_bin(value, block.new_producers ? *block.new_producers : state_history::producer_schedule{});

        lmdb::put(t, lmdb_inst->db, key, value);
        for (size_t i = 0; i < positions.size(); ++i)
            block_info_table->fields[i]->pos = {value.data() + positions[i], value.data() + value.size()};

        std::vector<char> index_key;
        for (auto& [_, index] : block_info_table->indexes) {
            index_key.clear();
            kv::append_table_index_key(index_key, block_info_table->short_name, index.name);
            fill_key(index_key, index);
            lmdb::put(t, lmdb_inst->db, index_key, key);
            lmdb::put(t, lmdb_inst->db, kv::make_table_index_ref_key(block_num, key, index_key), index_key);
        }
    } // receive_block

    void receive_deltas(lmdb::transaction& t, uint32_t block_num, input_buffer bin) {
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
                if (table_delta.rows.size() > 10000 && !(num_processed % 10000))
                    ilog(
                        "block ${b} ${t} ${n} of ${r}",
                        ("b", block_num)("t", table_delta.name)("n", num_processed)("r", table_delta.rows.size()));
                check_variant(row.data, *table.abi_type, 0u);
                delta_key.clear();
                kv::append_delta_key(delta_key, block_num, row.present, table.short_name);
                value.clear();
                abieos::native_to_bin(value, block_num);
                abieos::native_to_bin(value, row.present);
                for (auto& field : table.fields)
                    fill(value, row.data, *field);
                fill_key(delta_key, *table.delta_index);
                lmdb::put(t, lmdb_inst->db, delta_key, value);

                for (auto& [_, index] : table.indexes) {
                    index_key.clear();
                    kv::append_table_index_key(index_key, table.short_name, index.name);
                    fill_key(index_key, index);
                    kv::append_table_index_state_suffix(index_key, block_num, row.present);
                    lmdb::put(t, lmdb_inst->db, index_key, delta_key);
                    lmdb::put(t, lmdb_inst->db, kv::make_table_index_ref_key(block_num, delta_key, index_key), index_key);
                }
                ++num_processed;
            }
        }
    } // receive_deltas

    void receive_traces(lmdb::transaction& t, uint32_t block_num, input_buffer bin) {
        auto     num          = read_varuint32(bin);
        uint32_t num_ordinals = 0;
        for (uint32_t i = 0; i < num; ++i) {
            state_history::transaction_trace trace;
            bin_to_native(trace, bin);
            write_transaction_trace(t, block_num, num_ordinals, std::get<state_history::transaction_trace_v0>(trace));
        }
    }

    void write_transaction_trace(
        lmdb::transaction& t, uint32_t block_num, uint32_t& num_ordinals, const state_history::transaction_trace_v0& ttrace) {
        auto* failed = !ttrace.failed_dtrx_trace.empty()
                           ? &std::get<state_history::transaction_trace_v0>(ttrace.failed_dtrx_trace[0].recurse)
                           : nullptr;
        if (failed)
            write_transaction_trace(t, block_num, num_ordinals, *failed);
        uint32_t transaction_ordinal = ++num_ordinals;

        std::vector<char> key;
        kv::append_transaction_trace_key(key, block_num, ttrace.id);

        std::vector<char> value;
        abieos::native_to_bin(value, block_num);
        abieos::native_to_bin(value, transaction_ordinal);
        abieos::native_to_bin(value, failed ? failed->id : abieos::checksum256{});
        abieos::native_to_bin(value, ttrace.id);
        abieos::native_to_bin(value, (uint8_t)ttrace.status);
        abieos::native_to_bin(value, ttrace.cpu_usage_us);
        abieos::native_to_bin(value, ttrace.net_usage_words);
        abieos::native_to_bin(value, ttrace.elapsed);
        abieos::native_to_bin(value, ttrace.net_usage);
        abieos::native_to_bin(value, ttrace.scheduled);
        abieos::native_to_bin(value, ttrace.account_ram_delta.has_value());
        if (ttrace.account_ram_delta) {
            abieos::native_to_bin(value, ttrace.account_ram_delta->account);
            abieos::native_to_bin(value, ttrace.account_ram_delta->delta);
        }
        abieos::native_to_bin(value, ttrace.except ? *ttrace.except : "");
        abieos::native_to_bin(value, ttrace.error_code ? *ttrace.error_code : 0);

        // lmdb::put(t, lmdb_inst->db, key, value); // todo: indexes, including trim

        std::vector<char> index_key;
        for (auto& atrace : ttrace.action_traces)
            write_action_trace(t, block_num, ttrace, std::get<state_history::action_trace_v0>(atrace), key, value, index_key);
    }

    void write_action_trace(
        lmdb::transaction& t, uint32_t block_num, const state_history::transaction_trace_v0& ttrace,
        const state_history::action_trace_v0& atrace, std::vector<char>& key, std::vector<char>& value, std::vector<char>& index_key) {

        key.clear();
        kv::append_action_trace_key(key, block_num, ttrace.id, atrace.action_ordinal.value);
        value.clear();

        std::vector<uint32_t> positions;
        auto                  f = [&](const auto& x) {
            positions.push_back(value.size());
            abieos::native_to_bin(value, x);
        };

        f(block_num);
        f(ttrace.id);
        f((uint8_t)ttrace.status);
        f(atrace.action_ordinal);
        f(atrace.creator_action_ordinal);
        abieos::native_to_bin(value, atrace.receipt.has_value());
        if (atrace.receipt) {
            auto& receipt = std::get<state_history::action_receipt_v0>(*atrace.receipt);
            abieos::native_to_bin(value, receipt.receiver);
            abieos::native_to_bin(value, receipt.act_digest);
            abieos::native_to_bin(value, receipt.global_sequence);
            abieos::native_to_bin(value, receipt.recv_sequence);
            abieos::native_to_bin(value, receipt.code_sequence);
            abieos::native_to_bin(value, receipt.abi_sequence);
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

        abieos::native_to_bin(value, atrace.console);
        abieos::native_to_bin(value, atrace.except ? *atrace.except : std::string());
        lmdb::put(t, lmdb_inst->db, key, value);

        for (size_t i = 0; i < positions.size(); ++i)
            action_trace_table->fields[i]->pos = {value.data() + positions[i], value.data() + value.size()};

        for (auto& [_, index] : action_trace_table->indexes) {
            index_key.clear();
            kv::append_table_index_key(index_key, action_trace_table->short_name, index.name);
            fill_key(index_key, index);
            lmdb::put(t, lmdb_inst->db, index_key, key);
            lmdb::put(t, lmdb_inst->db, kv::make_table_index_ref_key(block_num, key, index_key), index_key);
        }

        // todo: receipt_auth_sequence
        // todo: authorization
        // todo: account_ram_deltas
    }

    template <typename F>
    void for_each_row_in_block(lmdb::transaction& t, uint32_t block_num, F f) {
        auto row_bound = kv::make_table_row_key(block_num);
        lmdb::for_each(t, lmdb_inst->db, row_bound, row_bound, [&](auto k, auto row_content) {
            k.pos += row_bound.size();
            auto table_name = kv::bin_to_native_key<abieos::name>(k);
            auto pk         = k;
            return f(table_name, pk);
        });
    }

    template <typename F>
    void for_each_delta_in_block(lmdb::transaction& t, uint32_t block_num, F f) {
        std::vector<char> delta_bound;
        kv::append_delta_key(delta_bound, block_num);
        lmdb::for_each(t, lmdb_inst->db, delta_bound, delta_bound, [&](auto k, auto row_content) {
            k.pos += delta_bound.size();
            auto table_name = kv::bin_to_native_key<abieos::name>(k);
            auto present    = kv::bin_to_native_key<bool>(k);
            auto pk         = k;
            return f(table_name, present, pk);
        });
    }

    template <typename F>
    void for_each_row_trim(lmdb::transaction& t, abieos::name table_name, abieos::input_buffer pk, uint32_t block_num, F f) {
        std::vector<char> trim_bound;
        kv::append_table_index_key(trim_bound, table_name, "trim"_n);
        kv::native_to_bin_key<uint32_t>(trim_bound, block_num);
        lmdb::for_each(t, lmdb_inst->db, trim_bound, trim_bound, [&](auto k, auto v) { return f(v); });
    }

    template <typename F>
    void for_each_delta_trim(lmdb::transaction& t, abieos::name table_name, abieos::input_buffer pk, uint32_t max_block_num, F f) {
        std::vector<char> trim_bound;
        kv::append_table_index_key(trim_bound, table_name, "trim"_n);
        trim_bound.insert(trim_bound.end(), pk.pos, pk.end);
        auto trim_lower_bound = trim_bound;
        kv::native_to_bin_key<uint32_t>(trim_lower_bound, ~max_block_num);

        lmdb::for_each(t, lmdb_inst->db, trim_lower_bound, trim_bound, [&](auto k, auto v) {
            k.pos += trim_bound.size();
            auto block_num = ~kv::bin_to_native_key<uint32_t>(k);
            auto present   = !kv::bin_to_native_key<bool>(k);
            return f(block_num, present);
        });
    }

    void remove_row(lmdb::transaction& t, abieos::name table_name, uint32_t block_num, std::vector<char> key) {
        auto index_ref_bounds = kv::make_table_index_ref_key(block_num, key);
        lmdb::for_each(t, lmdb_inst->db, index_ref_bounds, index_ref_bounds, [&](auto k, auto v) {
            lmdb::check(mdb_del(t.tx, lmdb_inst->db.db, lmdb::addr(lmdb::to_const_val(v)), nullptr), "remove_row (1): ");
            lmdb::check(mdb_del(t.tx, lmdb_inst->db.db, lmdb::addr(lmdb::to_const_val(k)), nullptr), "remove_row (2): ");
            return true;
        });
        lmdb::check(mdb_del(t.tx, lmdb_inst->db.db, lmdb::addr(lmdb::to_const_val(key)), nullptr), "remove_row (3): ");
    }

    void remove_delta(lmdb::transaction& t, abieos::name table_name, uint32_t block_num, bool present, abieos::input_buffer pk) {
        std::vector<char> delta_key;
        kv::append_delta_key(delta_key, block_num, present, table_name);
        delta_key.insert(delta_key.end(), pk.pos, pk.end);
        remove_row(t, table_name, block_num, delta_key);
    }

    void trim(lmdb::transaction& t) {
        if (!config->enable_trim)
            return;
        auto end_trim = std::min(head, irreversible);
        if (first >= end_trim)
            return;
        ilog("trim: ${b} - ${e}", ("b", first)("e", end_trim));

        uint64_t num_rows_removed   = 0;
        uint64_t num_deltas_removed = 0;
        for (auto block_num = first; block_num < end_trim; ++block_num) {
            if (end_trim - first >= 400 && !(block_num % 100)) {
                ilog("trim: removed ${r} rows and ${d} deltas so far", ("r", num_rows_removed)("d", num_deltas_removed));
                ilog("trim ${x}", ("x", block_num));
            }
            for_each_row_in_block(t, block_num, [&](auto row_table_name, auto row_pk) {
                for_each_row_trim(t, row_table_name, row_pk, block_num, [&](auto table_key) {
                    remove_row(t, row_table_name, block_num, {table_key.pos, table_key.end});
                    ++num_rows_removed;
                    return true;
                });
                return true;
            });
            for_each_delta_in_block(t, block_num + 1, [&](auto delta_table_name, auto delta_present, auto delta_pk) {
                for_each_delta_trim(t, delta_table_name, delta_pk, block_num + 1, [&](auto trim_block_num, auto trim_present) {
                    if (trim_block_num == block_num + 1)
                        return true;
                    if (trim_block_num > block_num + 1)
                        throw std::runtime_error("found unexpected block in trim search: " + std::to_string(trim_block_num));
                    remove_delta(t, delta_table_name, trim_block_num, trim_present, delta_pk);
                    ++num_deltas_removed;
                    return true;
                });
                return true;
            });
            lmdb::check(
                mdb_del(t.tx, lmdb_inst->db.db, lmdb::addr(lmdb::to_const_val(kv::make_received_block_key(block_num))), nullptr), "trim: ");
        }

        ilog("trim: removed ${r} rows and ${d} deltas", ("r", num_rows_removed)("d", num_deltas_removed));
        first = end_trim;
    }

    const abi_type& get_type(const std::string& name) { return connection->get_type(name); }

    void closed() override {
        if (my)
            my->session.reset();
    }

    ~flm_session() { ilog("fill_lmdb_plugin stopped"); }
}; // flm_session

static abstract_plugin& _fill_lmdb_plugin = app().register_plugin<fill_lmdb_plugin>();

fill_lmdb_plugin_impl::~fill_lmdb_plugin_impl() {
    if (session)
        session->my = nullptr;
}

fill_lmdb_plugin::fill_lmdb_plugin()
    : my(std::make_shared<fill_lmdb_plugin_impl>()) {}

fill_lmdb_plugin::~fill_lmdb_plugin() {}

void fill_lmdb_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto clop = cli.add_options();
    clop("flm-check", "Check database");
}

void fill_lmdb_plugin::plugin_initialize(const variables_map& options) {
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
        my->config->enable_check = options.count("flm-check");
    }
    FC_LOG_AND_RETHROW()
}

void fill_lmdb_plugin::plugin_startup() {
    my->session = std::make_shared<flm_session>(my.get());
    if (my->config->enable_check)
        my->session->check();
    my->session->connect(app().get_io_service());
}

void fill_lmdb_plugin::plugin_shutdown() {
    if (my->session)
        my->session->connection->close();
}
