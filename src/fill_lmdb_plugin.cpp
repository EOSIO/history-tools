// copyright defined in LICENSE.txt

#include "fill_lmdb_plugin.hpp"
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

namespace asio      = boost::asio;
namespace bpo       = boost::program_options;
namespace lmdb      = state_history::lmdb;
namespace websocket = boost::beast::websocket;

using asio::ip::tcp;
using boost::beast::flat_buffer;
using boost::system::error_code;

struct flm_session;

struct lmdb_table;

struct lmdb_field {
    std::string                 name      = {};
    const abieos::abi_field*    abi_field = {};
    const lmdb::lmdb_type*      lmdb_type = {};
    std::unique_ptr<lmdb_table> array_of  = {};
    abieos::input_buffer        pos       = {}; // temporary filled by fill()
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

struct fill_lmdb_config {
    std::string host;
    std::string port;
    uint32_t    skip_to      = 0;
    uint32_t    stop_before  = 0;
    bool        enable_trim  = false;
    bool        enable_check = false;
};

struct fill_lmdb_plugin_impl : std::enable_shared_from_this<fill_lmdb_plugin_impl> {
    std::shared_ptr<fill_lmdb_config> config = std::make_shared<fill_lmdb_config>();
    std::shared_ptr<::flm_session>    session;

    ~fill_lmdb_plugin_impl();
};

struct flm_session : std::enable_shared_from_this<flm_session> {
    fill_lmdb_plugin_impl*                    my = nullptr;
    std::shared_ptr<fill_lmdb_config>         config;
    std::shared_ptr<::lmdb_inst>              lmdb_inst = app().find_plugin<lmdb_plugin>()->get_lmdb_inst();
    std::optional<lmdb::transaction>          active_tx;
    tcp::resolver                             resolver;
    websocket::stream<tcp::socket>            stream;
    bool                                      received_abi       = false;
    std::map<std::string, lmdb_table>         tables             = {};
    lmdb_table*                               block_info_table   = {};
    lmdb_table*                               action_trace_table = {};
    bool                                      created_trim       = false;
    std::optional<state_history::fill_status> current_db_status  = {};
    uint32_t                                  head               = 0;
    abieos::checksum256                       head_id            = {};
    uint32_t                                  irreversible       = 0;
    abieos::checksum256                       irreversible_id    = {};
    uint32_t                                  first              = 0;
    uint32_t                                  first_bulk         = 0;
    abi_def                                   abi                = {};
    std::map<std::string, abi_type>           abi_types          = {};

    flm_session(fill_lmdb_plugin_impl* my, asio::io_context& ioc)
        : my(my)
        , config(my->config)
        , resolver(ioc)
        , stream(ioc) {

        ilog("connect to lmdb");
        stream.binary(true);
        stream.read_message_max(1024 * 1024 * 1024);
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

        std::vector<char> bin;
        uint32_t          expected = first;
        for_each_subkey(t, lmdb_inst->db, lmdb::make_block_key(0), lmdb::make_block_key(0xffff'ffff), [&](auto& k, auto v) {
            bin.clear();
            abieos::input_buffer ib1{k.data(), k.data() + k.size()};
            lmdb::bin_to_bin_key<uint8_t>(bin, ib1);
            lmdb::bin_to_bin_key<uint32_t>(bin, ib1);
            abieos::input_buffer ib2{bin.data(), bin.data() + bin.size()};
            if (lmdb::bin_to_key_tag(ib2) != lmdb::key_tag::block)
                throw std::runtime_error("This shouldn't happen (1)");
            auto block_index = abieos::bin_to_native<uint32_t>(ib2);
            if (block_index == first || block_index == head || !(block_index % 10'000))
                ilog("Found records for block ${b}", ("b", block_index));
            if (block_index != expected)
                throw std::runtime_error("Saw block_index " + std::to_string(block_index) + " but expected " + std::to_string(expected));
            ++expected;
            return true;
        });
        if (expected - 1 != head)
            throw std::runtime_error("Found head " + std::to_string(expected - 1) + " but fill_status.head = " + std::to_string(head));

        ilog("verifying table_index keys reference existing records");
        uint32_t num_ti_keys = 0;
        for_each(t, lmdb_inst->db, lmdb::make_table_index_key(), lmdb::make_table_index_key(), [&](auto k, auto v) {
            bin.clear();
            if (!((++num_ti_keys) % 1'000'000))
                ilog("Found ${n} table_index keys", ("n", num_ti_keys));
            if (!lmdb::exists(t, lmdb_inst->db, lmdb::to_const_val(v)))
                throw std::runtime_error("A table_index key references a missing record");
            return true;
        });
        ilog("Found ${n} table_index keys", ("n", num_ti_keys));

        ilog("verifying table_index_ref keys reference existing table_index records");
        uint32_t num_ti_ref_keys = 0;
        for_each(t, lmdb_inst->db, lmdb::make_table_index_ref_key(), lmdb::make_table_index_ref_key(), [&](auto k, auto v) {
            bin.clear();
            if (!((++num_ti_ref_keys) % 1'000'000))
                ilog("Found ${n} table_index_ref keys", ("n", num_ti_ref_keys));
            if (!lmdb::exists(t, lmdb_inst->db, lmdb::to_const_val(v)))
                throw std::runtime_error("A table_index_ref key references a missing table_index record");
            return true;
        });
        ilog("Found ${n} table_index_ref keys", ("n", num_ti_ref_keys));
    }

    void start() {
        ilog("connect to ${h}:${p}", ("h", config->host)("p", config->port));
        resolver.async_resolve(
            config->host, config->port, [self = shared_from_this(), this](error_code ec, tcp::resolver::results_type results) {
                callback(ec, "resolve", [&] {
                    asio::async_connect(
                        stream.next_layer(), results.begin(), results.end(), [self = shared_from_this(), this](error_code ec, auto&) {
                            callback(ec, "connect", [&] {
                                stream.async_handshake(config->host, "/", [self = shared_from_this(), this](error_code ec) {
                                    callback(ec, "handshake", [&] { //
                                        start_read();
                                    });
                                });
                            });
                        });
                });
            });
    }

    void start_read() {
        auto in_buffer = std::make_shared<flat_buffer>();
        stream.async_read(*in_buffer, [self = shared_from_this(), this, in_buffer](error_code ec, size_t) {
            callback(ec, "async_read", [&] {
                if (!received_abi)
                    receive_abi(in_buffer);
                else {
                    if (!receive_result(in_buffer)) {
                        close();
                        return;
                    }
                }
                start_read();
            });
        });
    }

    void fill_fields(lmdb_table& table, const std::string& base_name, const abieos::abi_field& abi_field) {
        if (abi_field.type->filled_struct) {
            for (auto& f : abi_field.type->fields)
                fill_fields(table, base_name + abi_field.name + "_", f);
        } else if (abi_field.type->filled_variant && abi_field.type->fields.size() == 1 && abi_field.type->fields[0].type->filled_struct) {
            for (auto& f : abi_field.type->fields[0].type->fields)
                fill_fields(table, base_name + abi_field.name + "_", f);
        } else {
            bool array_of_struct = abi_field.type->array_of && abi_field.type->array_of->filled_struct;
            auto field_name      = base_name + abi_field.name;
            if (table.field_map.find(field_name) != table.field_map.end())
                throw std::runtime_error("duplicate field " + field_name + " in table " + table.name);

            auto* raw_type = abi_field.type;
            if (raw_type->optional_of)
                raw_type = raw_type->optional_of;
            if (raw_type->array_of)
                raw_type = raw_type->array_of;
            auto type_it = lmdb::abi_type_to_lmdb_type.find(raw_type->name);
            if (type_it == lmdb::abi_type_to_lmdb_type.end() && !array_of_struct)
                throw std::runtime_error("don't know lmdb type for abi type: " + raw_type->name);

            table.fields.push_back(std::make_unique<lmdb_field>());
            auto* f                     = table.fields.back().get();
            table.field_map[field_name] = f;
            f->name                     = field_name;
            f->abi_field                = &abi_field;
            f->lmdb_type                = array_of_struct ? nullptr : &type_it->second;

            if (array_of_struct) {
                f->array_of = std::make_unique<lmdb_table>(lmdb_table{.name = field_name, .abi_type = abi_field.type->array_of});
                for (auto& g : abi_field.type->array_of->fields)
                    fill_fields(*f->array_of, base_name, g);
            }
        }
    }

    void add_table(const std::string& table_name, const std::string& table_type, const jarray& key_names) {
        auto table_name_it = lmdb::table_names.find(table_name);
        if (table_name_it == lmdb::table_names.end())
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
        for (auto& key : key_names) {
            auto& field_name = std::get<std::string>(key.value);
            auto  it         = table.field_map.find(field_name);
            if (it == table.field_map.end())
                throw std::runtime_error("table \"" + table_name + "\" key \"" + field_name + "\" not found");
            table.delta_index->fields.push_back(it->second);
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
        fill_fields(*block_info_table, "", abieos::abi_field{"block_index", &get_type("uint32")});
        fill_fields(*block_info_table, "", abieos::abi_field{"block_id", &get_type("checksum256")});
        fill_fields(*block_info_table, "", abieos::abi_field{"timestamp", &get_type("block_timestamp_type")});
        fill_fields(*block_info_table, "", abieos::abi_field{"producer", &get_type("name")});
        fill_fields(*block_info_table, "", abieos::abi_field{"confirmed", &get_type("uint16")});
        fill_fields(*block_info_table, "", abieos::abi_field{"previous", &get_type("checksum256")});
        fill_fields(*block_info_table, "", abieos::abi_field{"transaction_mroot", &get_type("checksum256")});
        fill_fields(*block_info_table, "", abieos::abi_field{"action_mroot", &get_type("checksum256")});
        fill_fields(*block_info_table, "", abieos::abi_field{"schedule_version", &get_type("uint32")});

        action_trace_table             = &tables["action_trace"];
        action_trace_table->name       = "action_trace";
        action_trace_table->short_name = "atrace"_n;
        fill_fields(*action_trace_table, "", abieos::abi_field{"block_index", &get_type("uint32")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"transaction_id", &get_type("checksum256")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"action_index", &get_type("uint32")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"parent_action_index", &get_type("uint32")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"transaction_status", &get_type("uint8")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"receipt_receiver", &get_type("name")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"receipt_act_digest", &get_type("checksum256")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"receipt_global_sequence", &get_type("uint64")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"receipt_recv_sequence", &get_type("uint64")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"receipt_code_sequence", &get_type("varuint32")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"receipt_abi_sequence", &get_type("varuint32")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"account", &get_type("name")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"name", &get_type("name")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"data", &get_type("bytes")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"context_free", &get_type("bool")});
        fill_fields(*action_trace_table, "", abieos::abi_field{"elapsed", &get_type("int64")});
    } // init_tables

    void init_indexes() {
        // todo: verify index set in config matches index set in db
        auto& c = lmdb_inst->query_config;
        for (auto& [query_name, query] : c.query_map) {
            auto table_it = tables.find(query->_table);
            if (table_it == tables.end())
                throw std::runtime_error("can't find table " + query->_table);
            auto& table = table_it->second;

            auto index_it = table.indexes.find(query_name);
            if (index_it != table.indexes.end())
                throw std::runtime_error("duplicate index " + query->_table + "." + (std::string)query_name);
            auto& index = table.indexes[query_name];
            index.name  = query_name;

            for (auto& key : query->sort_keys) {
                auto field_it = table.field_map.find(key.name);
                if (field_it == table.field_map.end())
                    throw std::runtime_error("can't find " + query->_table + "." + key.name);
                index.fields.push_back(field_it->second);
            }
        }
    }

    void receive_abi(const std::shared_ptr<flat_buffer>& p) {
        auto data = p->data();
        json_to_native(abi, std::string_view{(const char*)data.data(), data.size()});
        check_abi_version(abi.version);
        abi_types    = create_contract(abi).abi_types;
        received_abi = true;

        init_tables(std::string_view{(const char*)data.data(), data.size()});
        init_indexes();

        lmdb::transaction t{lmdb_inst->lmdb_env, true};
        load_fill_status(t);
        auto positions = get_positions(t);
        truncate(t, head + 1);
        t.commit();

        send_request(positions);
    }

    void load_fill_status(lmdb::transaction& t) {
        current_db_status = lmdb::get<state_history::fill_status>(t, lmdb_inst->db, lmdb::make_fill_status_key(), false);
        if (!current_db_status)
            return;
        head            = current_db_status->head;
        head_id         = current_db_status->head_id;
        irreversible    = current_db_status->irreversible;
        irreversible_id = current_db_status->irreversible_id;
        first           = current_db_status->first;
    }

    void check_conflicts(lmdb::transaction& t) {
        auto r = lmdb::get<state_history::fill_status>(t, lmdb_inst->db, lmdb::make_fill_status_key(), false);
        if ((bool)r != (bool)current_db_status || (r && *r != *current_db_status))
            throw std::runtime_error("Another process is filling this database");
    }

    jarray get_positions(lmdb::transaction& t) {
        jarray result;
        if (head) {
            for (uint32_t i = irreversible; i <= head; ++i) {
                auto rb = lmdb::get<lmdb::received_block>(t, lmdb_inst->db, lmdb::make_received_block_key(i));
                result.push_back(jvalue{jobject{
                    {{"block_num"s}, jvalue{std::to_string(rb->block_index)}},
                    {{"block_id"s}, jvalue{(std::string)rb->block_id}},
                }});
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
        put(t, lmdb_inst->db, lmdb::make_fill_status_key(), *current_db_status, true);
    }

    void truncate(lmdb::transaction& t, uint32_t block) {
        for_each(t, lmdb_inst->db, lmdb::make_block_key(block), lmdb::make_block_key(), [&](auto k, auto v) {
            lmdb::check(mdb_del(t.tx, lmdb_inst->db.db, lmdb::addr(lmdb::to_const_val(k)), nullptr), "truncate (1): ");
            return true;
        });
        for_each(t, lmdb_inst->db, lmdb::make_table_index_ref_key(block), lmdb::make_table_index_ref_key(), [&](auto k, auto v) {
            std::vector<char> k2{k.pos, k.end};
            lmdb::check(mdb_del(t.tx, lmdb_inst->db.db, lmdb::addr(lmdb::to_const_val(v)), nullptr), "truncate (2): ");
            lmdb::check(mdb_del(t.tx, lmdb_inst->db.db, lmdb::addr(lmdb::to_const_val(k2)), nullptr), "truncate (3): ");
            return true;
        });

        auto rb = lmdb::get<lmdb::received_block>(t, lmdb_inst->db, lmdb::make_received_block_key(block - 1), false);
        if (!rb) {
            head    = 0;
            head_id = {};
        } else {
            head    = block - 1;
            head_id = rb->block_id;
        }
        first = std::min(first, head);
    }

    bool receive_result(const std::shared_ptr<flat_buffer>& p) {
        auto         data = p->data();
        input_buffer bin{(const char*)data.data(), (const char*)data.data() + data.size()};
        check_variant(bin, get_type("result"), "get_blocks_result_v0");

        state_history::get_blocks_result_v0 result;
        bin_to_native(result, bin);

        if (!result.this_block)
            return true;

        if (config->stop_before && result.this_block->block_num >= config->stop_before) {
            ilog("block ${b}: stop requested", ("b", result.this_block->block_num));
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

            if (!(result.this_block->block_num % 200) || result.this_block->block_num + 4 >= result.last_irreversible.block_num)
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

            put(*active_tx, lmdb_inst->db, lmdb::make_received_block_key(result.this_block->block_num),
                lmdb::received_block{result.this_block->block_num, result.this_block->block_id});

            if (!(result.this_block->block_num % 200) || result.this_block->block_num + 4 >= result.last_irreversible.block_num) {
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
        } else if (field.array_of) {
            uint32_t n = read_varuint32(src);
            abieos::push_varuint32(dest, n);
            for (uint32_t i = 0; i < n; ++i) {
                for (auto& f : field.array_of->fields)
                    fill(dest, src, *f);
            }
        } else {
            if (!field.lmdb_type->bin_to_bin)
                throw std::runtime_error("don't know how to process " + field.abi_field->type->name);
            if (field.abi_field->type->optional_of) {
                bool exists = read_raw<bool>(src);
                abieos::push_raw<bool>(dest, exists);
                if (!exists)
                    return;
            }
            field.lmdb_type->bin_to_bin(dest, src);
        }
    } // fill

    void fill_key(std::vector<char>& dest, lmdb_index& index) {
        for (auto& field : index.fields) {
            auto pos = field->pos;
            field->lmdb_type->bin_to_bin_key(dest, pos);
        }
    }

    void receive_block(uint32_t block_index, const checksum256& block_id, input_buffer bin, lmdb::transaction& t) {
        state_history::signed_block block;
        bin_to_native(block, bin);
        auto              key = lmdb::make_block_info_key(block_index);
        std::vector<char> value;

        std::vector<uint32_t> positions;
        auto                  f = [&](const auto& x) {
            positions.push_back(value.size());
            abieos::native_to_bin(value, x);
        };

        f(block_index);
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
            lmdb::append_table_index_key(index_key, block_info_table->short_name, index.name);
            fill_key(index_key, index);
            lmdb::put(t, lmdb_inst->db, index_key, key);
            lmdb::put(t, lmdb_inst->db, lmdb::make_table_index_ref_key(block_index, index_key), index_key);
        }
    } // receive_block

    void receive_deltas(lmdb::transaction& t, uint32_t block_num, input_buffer buf) {
        auto              data = zlib_decompress(buf);
        input_buffer      bin{data.data(), data.data() + data.size()};
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
                lmdb::append_delta_key(delta_key, block_num, row.present, table.short_name);
                value.clear();
                abieos::native_to_bin(value, block_num);
                abieos::native_to_bin(value, row.present);
                for (auto& field : table.fields)
                    fill(value, row.data, *field);
                fill_key(delta_key, *table.delta_index);
                lmdb::put(t, lmdb_inst->db, delta_key, value);

                for (auto& [_, index] : table.indexes) {
                    index_key.clear();
                    lmdb::append_table_index_key(index_key, table.short_name, index.name);
                    fill_key(index_key, index);
                    lmdb::append_table_index_state_suffix(index_key, block_num, row.present);
                    lmdb::put(t, lmdb_inst->db, index_key, delta_key);
                    lmdb::put(t, lmdb_inst->db, lmdb::make_table_index_ref_key(block_num, index_key), index_key);
                }
                ++num_processed;
            }
        }
    } // receive_deltas

    void receive_traces(lmdb::transaction& t, uint32_t block_num, input_buffer buf) {
        auto         data = zlib_decompress(buf);
        input_buffer bin{data.data(), data.data() + data.size()};
        auto         num = read_varuint32(bin);
        for (uint32_t i = 0; i < num; ++i) {
            state_history::transaction_trace trace;
            bin_to_native(trace, bin);
            write_transaction_trace(t, block_num, trace);
        }
    }

    void write_transaction_trace(lmdb::transaction& t, uint32_t block_num, const state_history::transaction_trace& ttrace) {
        std::vector<char> key;
        lmdb::append_transaction_trace_key(key, block_num, ttrace.id);

        std::vector<char> value;
        abieos::native_to_bin(value, block_num);
        abieos::native_to_bin(value, ttrace.failed_dtrx_trace.empty() ? abieos::checksum256{} : ttrace.failed_dtrx_trace[0].id);
        abieos::native_to_bin(value, ttrace.id);
        abieos::native_to_bin(value, (uint8_t)ttrace.status);
        abieos::native_to_bin(value, ttrace.cpu_usage_us);
        abieos::native_to_bin(value, ttrace.net_usage_words);
        abieos::native_to_bin(value, ttrace.elapsed);
        abieos::native_to_bin(value, ttrace.net_usage);
        abieos::native_to_bin(value, ttrace.scheduled);
        abieos::native_to_bin(value, ttrace.except ? *ttrace.except : "");

        lmdb::put(t, lmdb_inst->db, key, value);

        uint32_t          prev_action_index = 0;
        std::vector<char> index_key;
        for (auto& atrace : ttrace.action_traces)
            write_action_trace(t, block_num, ttrace, atrace, 0, prev_action_index, key, value, index_key);
    }

    void write_action_trace(
        lmdb::transaction& t, uint32_t block_num, const state_history::transaction_trace& ttrace, const state_history::action_trace& atrace,
        uint32_t parent_action_index, uint32_t& prev_action_index, std::vector<char>& key, std::vector<char>& value,
        std::vector<char>& index_key) {

        auto action_index = ++prev_action_index;

        key.clear();
        lmdb::append_action_trace_key(key, block_num, ttrace.id, action_index);

        value.clear();

        std::vector<uint32_t> positions;
        auto                  f = [&](const auto& x) {
            positions.push_back(value.size());
            abieos::native_to_bin(value, x);
        };

        f(block_num);
        f(ttrace.id);
        f(action_index);
        f(parent_action_index);
        f((uint8_t)ttrace.status);
        f(atrace.receipt_receiver);
        f(atrace.receipt_act_digest);
        f(atrace.receipt_global_sequence);
        f(atrace.receipt_recv_sequence);
        f(atrace.receipt_code_sequence);
        f(atrace.receipt_abi_sequence);
        f(atrace.account);
        f(atrace.name);
        f(atrace.data);
        f(atrace.context_free);
        f(atrace.elapsed);

        abieos::native_to_bin(value, atrace.console);
        abieos::native_to_bin(value, atrace.except ? *atrace.except : std::string());
        lmdb::put(t, lmdb_inst->db, key, value);

        for (size_t i = 0; i < positions.size(); ++i)
            action_trace_table->fields[i]->pos = {value.data() + positions[i], value.data() + value.size()};

        for (auto& [_, index] : action_trace_table->indexes) {
            index_key.clear();
            lmdb::append_table_index_key(index_key, action_trace_table->short_name, index.name);
            fill_key(index_key, index);
            lmdb::put(t, lmdb_inst->db, index_key, key);
            lmdb::put(t, lmdb_inst->db, lmdb::make_table_index_ref_key(block_num, index_key), index_key);
        }

        // todo: receipt_auth_sequence
        // todo: authorization
        // todo: account_ram_deltas

        for (auto& child : atrace.inline_traces)
            write_action_trace(t, block_num, ttrace, child, action_index, prev_action_index, key, value, index_key);
    }

    void trim() {
        if (!config->enable_trim)
            return;
        auto end_trim = std::min(head, irreversible);
        if (first >= end_trim)
            return;
        ilog("trim  ${b} - ${e}", ("b", first)("e", end_trim));
        // todo
        ilog("      done");
        first = end_trim;
    }

    void send_request(const jarray& positions) {
        send(jvalue{jarray{{"get_blocks_request_v0"s},
                           {jobject{
                               {{"start_block_num"s}, {std::to_string(std::max(config->skip_to, head + 1))}},
                               {{"end_block_num"s}, {"4294967295"s}},
                               {{"max_messages_in_flight"s}, {"4294967295"s}},
                               {{"have_positions"s}, {positions}},
                               {{"irreversible_only"s}, {false}},
                               {{"fetch_block"s}, {true}},
                               {{"fetch_traces"s}, {true}},
                               {{"fetch_deltas"s}, {true}},
                           }}}});
    }

    const abi_type& get_type(const std::string& name) {
        auto it = abi_types.find(name);
        if (it == abi_types.end())
            throw std::runtime_error("unknown type "s + name);
        return it->second;
    }

    void send(const jvalue& value) {
        auto bin = std::make_shared<std::vector<char>>();
        json_to_bin(*bin, &get_type("request"), value);
        stream.async_write(
            asio::buffer(*bin), [self = shared_from_this(), bin, this](error_code ec, size_t) { callback(ec, "async_write", [&] {}); });
    }

    void check_variant(input_buffer& bin, const abi_type& type, uint32_t expected) {
        auto index = read_varuint32(bin);
        if (!type.filled_variant)
            throw std::runtime_error(type.name + " is not a variant"s);
        if (index >= type.fields.size())
            throw std::runtime_error("expected "s + type.fields[expected].name + " got " + std::to_string(index));
        if (index != expected)
            throw std::runtime_error("expected "s + type.fields[expected].name + " got " + type.fields[index].name);
    }

    void check_variant(input_buffer& bin, const abi_type& type, const char* expected) {
        auto index = read_varuint32(bin);
        if (!type.filled_variant)
            throw std::runtime_error(type.name + " is not a variant"s);
        if (index >= type.fields.size())
            throw std::runtime_error("expected "s + expected + " got " + std::to_string(index));
        if (type.fields[index].name != expected)
            throw std::runtime_error("expected "s + expected + " got " + type.fields[index].name);
    }

    template <typename F>
    void catch_and_close(F f) {
        try {
            f();
        } catch (const std::exception& e) {
            elog("${e}", ("e", e.what()));
            close();
        } catch (...) {
            elog("unknown exception");
            close();
        }
    }

    template <typename F>
    void callback(error_code ec, const char* what, F f) {
        if (ec)
            return on_fail(ec, what);
        catch_and_close(f);
    }

    void on_fail(error_code ec, const char* what) {
        try {
            elog("${w}: ${m}", ("w", what)("m", ec.message()));
            close();
        } catch (...) {
            elog("exception while closing");
        }
    }

    void close() {
        stream.next_layer().close();
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
    auto op   = cfg.add_options();
    auto clop = cli.add_options();
    op("flm-endpoint,e", bpo::value<std::string>()->default_value("localhost:8080"), "State-history endpoint to connect to (nodeos)");
    // op("flm-trim,t", "Trim history before irreversible");
    clop("flm-skip-to,k", bpo::value<uint32_t>(), "Skip blocks before [arg]");
    clop("flm-stop,x", bpo::value<uint32_t>(), "Stop before block [arg]");
    clop("flm-check", "Check database");
}

void fill_lmdb_plugin::plugin_initialize(const variables_map& options) {
    try {
        auto endpoint           = options.at("flm-endpoint").as<std::string>();
        auto port               = endpoint.substr(endpoint.find(':') + 1, endpoint.size());
        auto host               = endpoint.substr(0, endpoint.find(':'));
        my->config->host        = host;
        my->config->port        = port;
        my->config->skip_to     = options.count("flm-skip-to") ? options["flm-skip-to"].as<uint32_t>() : 0;
        my->config->stop_before = options.count("flm-stop") ? options["flm-stop"].as<uint32_t>() : 0;
        // my->config->enable_trim = options.count("flm-trim");
        my->config->enable_check = options.count("flm-check");
    }
    FC_LOG_AND_RETHROW()
}

void fill_lmdb_plugin::plugin_startup() {
    my->session = std::make_shared<flm_session>(my.get(), app().get_io_service());
    if (my->config->enable_check)
        my->session->check();
    my->session->start();
}

void fill_lmdb_plugin::plugin_shutdown() {
    if (my->session)
        my->session->close();
}
