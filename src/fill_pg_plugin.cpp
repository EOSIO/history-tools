// copyright defined in LICENSE.txt

// todo: transaction order within blocks. affects wasm-ql
// todo: trim: n behind head
// todo: trim: remove last !present

#include "state_history_pg.hpp"

#include "fill_pg_plugin.hpp"
#include "util.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <fc/exception/exception.hpp>

using namespace abieos;
using namespace appbase;
using namespace state_history;
using namespace state_history::pg;
using namespace std::literals;

namespace asio      = boost::asio;
namespace bpo       = boost::program_options;
namespace websocket = boost::beast::websocket;

using asio::ip::tcp;
using boost::beast::flat_buffer;
using boost::system::error_code;

struct table_stream {
    pqxx::connection  c;
    pqxx::work        t;
    pqxx::tablewriter writer;

    table_stream(const std::string& name)
        : t(c)
        , writer(t, name) {}
};

struct fpg_session;

struct fill_postgresql_config {
    std::string host;
    std::string port;
    std::string schema;
    uint32_t    skip_to       = 0;
    uint32_t    stop_before   = 0;
    bool        drop_schema   = false;
    bool        create_schema = false;
    bool        enable_trim   = false;
};

struct fill_postgresql_plugin_impl : std::enable_shared_from_this<fill_postgresql_plugin_impl> {
    std::shared_ptr<fill_postgresql_config> config = std::make_shared<fill_postgresql_config>();
    std::shared_ptr<fpg_session>            session;

    ~fill_postgresql_plugin_impl();
};

struct fpg_session : std::enable_shared_from_this<fpg_session> {
    fill_postgresql_plugin_impl*                         my = nullptr;
    std::shared_ptr<fill_postgresql_config>              config;
    std::optional<pqxx::connection>                      sql_connection;
    tcp::resolver                                        resolver;
    websocket::stream<tcp::socket>                       stream;
    bool                                                 received_abi    = false;
    bool                                                 created_trim    = false;
    uint32_t                                             head            = 0;
    std::string                                          head_id         = "";
    uint32_t                                             irreversible    = 0;
    std::string                                          irreversible_id = "";
    uint32_t                                             first           = 0;
    uint32_t                                             first_bulk      = 0;
    abi_def                                              abi{};
    std::map<std::string, abi_type>                      abi_types;
    std::map<std::string, std::unique_ptr<table_stream>> table_streams;

    fpg_session(fill_postgresql_plugin_impl* my, asio::io_context& ioc)
        : my(my)
        , config(my->config)
        , resolver(ioc)
        , stream(ioc) {

        ilog("connect to postgresql");
        sql_connection.emplace();
        stream.binary(true);
        stream.read_message_max(1024 * 1024 * 1024);
    }

    void start() {
        if (config->drop_schema) {
            pqxx::work t(*sql_connection);
            ilog("drop schema ${s}", ("s", t.quote_name(config->schema)));
            t.exec("drop schema if exists " + t.quote_name(config->schema) + " cascade");
            t.commit();
        }

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

    void receive_abi(const std::shared_ptr<flat_buffer>& p) {
        auto data = p->data();
        json_to_native(abi, std::string_view{(const char*)data.data(), data.size()});
        check_abi_version(abi.version);
        abi_types    = create_contract(abi).abi_types;
        received_abi = true;

        if (config->create_schema)
            create_tables();

        pqxx::work t(*sql_connection);
        load_fill_status(t);
        auto           positions = get_positions(t);
        pqxx::pipeline pipeline(t);
        truncate(t, pipeline, head + 1);
        pipeline.complete();
        t.commit();

        send_request(positions);
    }

    template <typename T>
    void create_table(pqxx::work& t, const std::string& name, const std::string& pk, std::string fields) {
        for_each_field((T*)nullptr, [&](const char* field_name, auto member_ptr) {
            using type              = typename decltype(member_ptr)::member_type;
            constexpr auto sql_type = sql_type_for<type>;
            if constexpr (is_known_type(sql_type)) {
                std::string type = sql_type.type;
                if (type == "transaction_status_type")
                    type = t.quote_name(config->schema) + "." + type;
                fields += ", "s + t.quote_name(field_name) + " " + type;
            }
        });

        std::string query =
            "create table " + t.quote_name(config->schema) + "." + t.quote_name(name) + "(" + fields + ", primary key (" + pk + "))";
        t.exec(query);
    }

    void fill_field(pqxx::work& t, const std::string& base_name, std::string& fields, abi_field& field) {
        if (field.type->filled_struct) {
            for (auto& f : field.type->fields)
                fill_field(t, base_name + field.name + "_", fields, f);
        } else if (field.type->filled_variant && field.type->fields.size() == 1 && field.type->fields[0].type->filled_struct) {
            for (auto& f : field.type->fields[0].type->fields)
                fill_field(t, base_name + field.name + "_", fields, f);
        } else if (field.type->array_of && field.type->array_of->filled_struct) {
            std::string sub_fields;
            for (auto& f : field.type->array_of->fields)
                fill_field(t, "", sub_fields, f);
            std::string query = "create type " + t.quote_name(config->schema) + "." + t.quote_name(field.type->array_of->name) + " as (" +
                                sub_fields.substr(2) + ")";
            t.exec(query);
            fields += ", " + t.quote_name(base_name + field.name) + " " + t.quote_name(config->schema) + "." +
                      t.quote_name(field.type->array_of->name) + "[]";
        } else {
            auto abi_type = field.type->name;
            if (abi_type.size() >= 1 && abi_type.back() == '?')
                abi_type.resize(abi_type.size() - 1);
            auto it = abi_type_to_sql_type.find(abi_type);
            if (it == abi_type_to_sql_type.end())
                throw std::runtime_error("don't know sql type for abi type: " + abi_type);
            std::string type = it->second.type;
            if (type == "transaction_status_type")
                type = t.quote_name(config->schema) + "." + type;
            fields += ", " + t.quote_name(base_name + field.name) + " " + it->second.type;
        }
    }; // fill_field

    void create_tables() {
        pqxx::work t(*sql_connection);

        ilog("create schema ${s}", ("s", t.quote_name(config->schema)));
        t.exec("create schema " + t.quote_name(config->schema));
        t.exec(
            "create type " + t.quote_name(config->schema) +
            ".transaction_status_type as enum('executed', 'soft_fail', 'hard_fail', 'delayed', 'expired')");
        t.exec(
            "create table " + t.quote_name(config->schema) +
            R"(.received_block ("block_index" bigint, "block_id" varchar(64), primary key("block_index")))");
        t.exec(
            "create table " + t.quote_name(config->schema) +
            R"(.fill_status ("head" bigint, "head_id" varchar(64), "irreversible" bigint, "irreversible_id" varchar(64), "first" bigint))");
        t.exec("create unique index on " + t.quote_name(config->schema) + R"(.fill_status ((true)))");
        t.exec("insert into " + t.quote_name(config->schema) + R"(.fill_status values (0, '', 0, '', 0))");

        // clang-format off
        create_table<action_trace_authorization>(   t, "action_trace_authorization",  "block_index, transaction_id, action_index, index",     "block_index bigint, transaction_id varchar(64), action_index integer, index integer, transaction_status " + t.quote_name(config->schema) + ".transaction_status_type");
        create_table<action_trace_auth_sequence>(   t, "action_trace_auth_sequence",  "block_index, transaction_id, action_index, index",     "block_index bigint, transaction_id varchar(64), action_index integer, index integer, transaction_status " + t.quote_name(config->schema) + ".transaction_status_type");
        create_table<action_trace_ram_delta>(       t, "action_trace_ram_delta",      "block_index, transaction_id, action_index, index",     "block_index bigint, transaction_id varchar(64), action_index integer, index integer, transaction_status " + t.quote_name(config->schema) + ".transaction_status_type");
        create_table<action_trace>(                 t, "action_trace",                "block_index, transaction_id, action_index",            "block_index bigint, transaction_id varchar(64), action_index integer, parent_action_index integer, transaction_status " + t.quote_name(config->schema) + ".transaction_status_type");
        create_table<transaction_trace>(            t, "transaction_trace",           "block_index, transaction_id",                          "block_index bigint, failed_dtrx_trace varchar(64)");
        // clang-format on

        for (auto& table : abi.tables) {
            auto& variant_type = get_type(table.type);
            if (!variant_type.filled_variant || variant_type.fields.size() != 1 || !variant_type.fields[0].type->filled_struct)
                throw std::runtime_error("don't know how to proccess " + variant_type.name);
            auto&       type   = *variant_type.fields[0].type;
            std::string fields = "block_index bigint, present bool";
            for (auto& field : type.fields)
                fill_field(t, "", fields, field);
            std::string keys = "block_index, present";
            for (auto& key : table.key_names)
                keys += ", " + t.quote_name(key);
            std::string query =
                "create table " + t.quote_name(config->schema) + "." + table.type + "(" + fields + ", primary key(" + keys + "))";
            t.exec(query);
        }

        t.exec(
            "create table " + t.quote_name(config->schema) +
            R"(.block_info(                   
                "block_index" bigint,
                "block_id" varchar(64),
                "timestamp" timestamp,
                "producer" varchar(13),
                "confirmed" integer,
                "previous" varchar(64),
                "transaction_mroot" varchar(64),
                "action_mroot" varchar(64),
                "schedule_version" bigint,
                "new_producers_version" bigint,
                "new_producers" )" +
            t.quote_name(config->schema) + R"(.producer_key[],
                primary key("block_index")))");

        t.commit();
    } // create_tables()

    void create_trim() {
        if (created_trim)
            return;
        pqxx::work t(*sql_connection);
        ilog("create_trim");
        for (auto& table : abi.tables) {
            if (table.key_names.empty())
                continue;
            std::string query = "create index if not exists " + table.type;
            for (auto& k : table.key_names)
                query += "_" + k;
            query += "_block_present_idx on " + t.quote_name(config->schema) + "." + t.quote_name(table.type) + "(\n";
            for (auto& k : table.key_names)
                query += "    " + t.quote_name(k) + ",\n";
            query += "    \"block_index\" desc,\n    \"present\" desc\n)";
            // std::cout << query << ";\n\n";
            t.exec(query);
        }

        std::string query = R"(
            drop function if exists chain.trim_history;
        )";
        // std::cout << query << "\n";
        t.exec(query);

        query = R"(
            create function chain.trim_history(
                prev_block_index bigint,
                irrev_block_index bigint
            ) returns void
            as $$
                declare
                    key_search record;
                begin)";

        static const char* const simple_cases[] = {
            "received_block",
            "action_trace_authorization",
            "action_trace_auth_sequence",
            "action_trace_ram_delta",
            "action_trace",
            "transaction_trace",
            "block_info",
        };

        for (const char* table : simple_cases) {
            query += R"(
                    delete from )" +
                     t.quote_name(config->schema) + "." + t.quote_name(table) + R"(
                    where
                        block_index >= prev_block_index
                        and block_index < irrev_block_index;
                    )";
        }

        for (auto& table : abi.tables) {
            if (table.key_names.empty()) {
                query += R"(
                    for key_search in
                        select
                            block_index
                        from
                            )" +
                         t.quote_name(config->schema) + "." + t.quote_name(table.type) + R"(
                        where
                            block_index > prev_block_index and block_index <= irrev_block_index
                        order by block_index desc, present desc
                        limit 1
                    loop
                        delete from )" +
                         t.quote_name(config->schema) + "." + t.quote_name(table.type) + R"(
                        where
                            block_index < key_search.block_index;
                    end loop;
                    )";
            } else {
                std::string keys, search_keys;
                for (auto& k : table.key_names) {
                    if (&k != &table.key_names.front()) {
                        keys += ", ";
                        search_keys += ", ";
                    }
                    keys += t.quote_name(k);
                    search_keys += "key_search." + t.quote_name(k);
                }
                query += R"(
                    for key_search in
                        select
                            distinct on()" +
                         keys + R"()
                            )" +
                         keys + R"(, block_index
                        from
                            )" +
                         t.quote_name(config->schema) + "." + t.quote_name(table.type) + R"(
                        where
                            block_index > prev_block_index and block_index <= irrev_block_index
                        order by )" +
                         keys + R"(, block_index desc, present desc
                    loop
                        delete from )" +
                         t.quote_name(config->schema) + "." + t.quote_name(table.type) + R"(
                        where
                            ()" +
                         keys + R"() = ()" + search_keys + R"()
                            and block_index < key_search.block_index;
                    end loop;
                    )";
            };
        }
        query += R"(
                end 
            $$ language plpgsql;
        )";

        // std::cout << query << "\n\n";
        t.exec(query);
        t.commit();
        created_trim = true;
    } // create_trim

    void load_fill_status(pqxx::work& t) {
        auto r =
            t.exec("select head, head_id, irreversible, irreversible_id, first from " + t.quote_name(config->schema) + ".fill_status")[0];
        head            = r[0].as<uint32_t>();
        head_id         = r[1].as<std::string>();
        irreversible    = r[2].as<uint32_t>();
        irreversible_id = r[3].as<std::string>();
        first           = r[4].as<uint32_t>();
    }

    jarray get_positions(pqxx::work& t) {
        jarray result;
        auto   rows = t.exec(
            "select block_index, block_id from " + t.quote_name(config->schema) + ".received_block where block_index >= " +
            std::to_string(irreversible) + " and block_index <= " + std::to_string(head) + " order by block_index");
        for (auto row : rows) {
            result.push_back(jvalue{jobject{
                {{"block_num"s}, {row[0].as<std::string>()}},
                {{"block_id"s}, {row[1].as<std::string>()}},
            }});
        }
        return result;
    }

    void write_fill_status(pqxx::work& t, pqxx::pipeline& pipeline) {
        std::string query = "update " + t.quote_name(config->schema) + ".fill_status set head=" + std::to_string(head) +
                            ", head_id=" + quote(head_id) + ", ";
        if (irreversible < head)
            query += "irreversible=" + std::to_string(irreversible) + ", irreversible_id=" + quote(irreversible_id);
        else
            query += "irreversible=" + std::to_string(head) + ", irreversible_id=" + quote(head_id);
        query += ", first=" + std::to_string(first);
        pipeline.insert(query);
    }

    void truncate(pqxx::work& t, pqxx::pipeline& pipeline, uint32_t block) {
        auto trunc = [&](const std::string& name) {
            pipeline.insert(
                "delete from " + t.quote_name(config->schema) + "." + t.quote_name(name) +
                " where block_index >= " + std::to_string(block));
        };
        trunc("received_block");
        trunc("action_trace_authorization");
        trunc("action_trace_auth_sequence");
        trunc("action_trace_ram_delta");
        trunc("action_trace");
        trunc("transaction_trace");
        trunc("block_info");
        for (auto& table : abi.tables)
            trunc(table.type);

        auto result = pipeline.retrieve(pipeline.insert(
            "select block_id from " + t.quote_name(config->schema) + ".received_block where block_index=" + std::to_string(block - 1)));
        if (result.empty()) {
            head    = 0;
            head_id = "";
        } else {
            head    = block - 1;
            head_id = result.front()[0].as<std::string>();
        }
        first = std::min(first, head);
    } // truncate

    bool receive_result(const std::shared_ptr<flat_buffer>& p) {
        auto         data = p->data();
        input_buffer bin{(const char*)data.data(), (const char*)data.data() + data.size()};
        check_variant(bin, get_type("result"), "get_blocks_result_v0");

        get_blocks_result_v0 result;
        bin_to_native(result, bin);

        if (!result.this_block)
            return true;

        bool bulk         = result.this_block->block_num + 4 < result.last_irreversible.block_num;
        bool large_deltas = false;
        if (!bulk && result.deltas && result.deltas->end - result.deltas->pos >= 10 * 1024 * 1024) {
            ilog("large deltas size: ${s}", ("s", uint64_t(result.deltas->end - result.deltas->pos)));
            bulk         = true;
            large_deltas = true;
        }

        if (config->stop_before && result.this_block->block_num >= config->stop_before) {
            close_streams();
            ilog("block ${b}: stop requested", ("b", result.this_block->block_num));
            return false;
        }

        if (result.this_block->block_num <= head) {
            close_streams();
            ilog("switch forks at block ${b}", ("b", result.this_block->block_num));
            bulk = false;
        }

        if (!bulk || large_deltas || !(result.this_block->block_num % 200))
            close_streams();
        if (table_streams.empty())
            trim();
        if (!bulk)
            ilog("block ${b}", ("b", result.this_block->block_num));

        pqxx::work     t(*sql_connection);
        pqxx::pipeline pipeline(t);
        if (result.this_block->block_num <= head)
            truncate(t, pipeline, result.this_block->block_num);
        if (!head_id.empty() && (!result.prev_block || (std::string)result.prev_block->block_id != head_id))
            throw std::runtime_error("prev_block does not match");
        if (result.block)
            receive_block(result.this_block->block_num, result.this_block->block_id, *result.block, bulk, t, pipeline);
        if (result.deltas)
            receive_deltas(result.this_block->block_num, *result.deltas, bulk, t, pipeline);
        if (result.traces)
            receive_traces(result.this_block->block_num, *result.traces, bulk, t, pipeline);

        head            = result.this_block->block_num;
        head_id         = (std::string)result.this_block->block_id;
        irreversible    = result.last_irreversible.block_num;
        irreversible_id = (std::string)result.last_irreversible.block_id;
        if (!first)
            first = head;
        if (!bulk)
            write_fill_status(t, pipeline);
        pipeline.insert(
            "insert into " + t.quote_name(config->schema) + ".received_block (block_index, block_id) values (" +
            std::to_string(result.this_block->block_num) + ", " + quote(std::string(result.this_block->block_id)) + ")");

        pipeline.complete();
        t.commit();
        if (large_deltas)
            close_streams();
        return true;
    } // receive_result()

    void write_stream(uint32_t block_num, pqxx::work& t, const std::string& name, const std::string& values) {
        if (!first_bulk)
            first_bulk = block_num;
        auto& ts = table_streams[name];
        if (!ts)
            ts = std::make_unique<table_stream>(t.quote_name(config->schema) + "." + t.quote_name(name));
        ts->writer.write_raw_line(values);
    }

    void close_streams() {
        if (table_streams.empty())
            return;
        for (auto& [_, ts] : table_streams) {
            ts->writer.complete();
            ts->t.commit();
            ts.reset();
        }
        table_streams.clear();

        pqxx::work     t(*sql_connection);
        pqxx::pipeline pipeline(t);
        write_fill_status(t, pipeline);
        pipeline.complete();
        t.commit();

        ilog("block ${b} - ${e}", ("b", first_bulk)("e", head));
        first_bulk = 0;
    }

    void fill_value(
        bool bulk, bool nested_bulk, pqxx::work& t, const std::string& base_name, std::string& fields, std::string& values,
        input_buffer& bin, abi_field& field) {
        if (field.type->filled_struct) {
            for (auto& f : field.type->fields)
                fill_value(bulk, nested_bulk, t, base_name + field.name + "_", fields, values, bin, f);
        } else if (field.type->filled_variant && field.type->fields.size() == 1 && field.type->fields[0].type->filled_struct) {
            auto v = read_varuint32(bin);
            if (v)
                throw std::runtime_error("invalid variant in " + field.type->name);
            for (auto& f : field.type->fields[0].type->fields)
                fill_value(bulk, nested_bulk, t, base_name + field.name + "_", fields, values, bin, f);
        } else if (field.type->array_of && field.type->array_of->filled_struct) {
            fields += ", " + t.quote_name(base_name + field.name);
            values += sep(bulk) + begin_array(bulk);
            uint32_t n = read_varuint32(bin);
            for (uint32_t i = 0; i < n; ++i) {
                if (i)
                    values += ",";
                values += begin_object_in_array(bulk);
                std::string struct_fields;
                std::string struct_values;
                for (auto& f : field.type->array_of->fields)
                    fill_value(bulk, true, t, "", struct_fields, struct_values, bin, f);
                if (bulk)
                    values += struct_values.substr(1);
                else
                    values += struct_values.substr(2);
                values += end_object_in_array(bulk);
            }
            values += end_array(bulk, t, config->schema, field.type->array_of->name);
        } else {
            auto abi_type    = field.type->name;
            bool is_optional = false;
            if (abi_type.size() >= 1 && abi_type.back() == '?') {
                is_optional = true;
                abi_type.resize(abi_type.size() - 1);
            }
            auto it = abi_type_to_sql_type.find(abi_type);
            if (it == abi_type_to_sql_type.end())
                throw std::runtime_error("don't know sql type for abi type: " + abi_type);
            if (!it->second.bin_to_sql)
                throw std::runtime_error("don't know how to process " + field.type->name);

            fields += ", " + t.quote_name(base_name + field.name);
            if (bulk) {
                if (nested_bulk)
                    values += ",";
                else
                    values += "\t";
                if (!is_optional || read_raw<bool>(bin))
                    values += it->second.bin_to_sql(*sql_connection, bulk, bin);
                else
                    values += "\\N";
            } else {
                if (!is_optional || read_raw<bool>(bin))
                    values += ", " + it->second.bin_to_sql(*sql_connection, bulk, bin);
                else
                    values += ", null";
            }
        }
    } // fill_value

    void
    receive_block(uint32_t block_index, const checksum256& block_id, input_buffer bin, bool bulk, pqxx::work& t, pqxx::pipeline& pipeline) {
        signed_block block;
        bin_to_native(block, bin);

        std::string fields = "block_index, block_id, timestamp, producer, confirmed, previous, transaction_mroot, action_mroot, "
                             "schedule_version, new_producers_version, new_producers";
        std::string values = sql_str(bulk, block_index) + sep(bulk) +                               //
                             sql_str(bulk, block_id) + sep(bulk) +                                  //
                             sql_str(bulk, block.timestamp) + sep(bulk) +                           //
                             sql_str(bulk, block.producer) + sep(bulk) +                            //
                             sql_str(bulk, block.confirmed) + sep(bulk) +                           //
                             sql_str(bulk, block.previous) + sep(bulk) +                            //
                             sql_str(bulk, block.transaction_mroot) + sep(bulk) +                   //
                             sql_str(bulk, block.action_mroot) + sep(bulk) +                        //
                             sql_str(bulk, block.schedule_version) + sep(bulk) +                    //
                             sql_str(bulk, block.new_producers ? block.new_producers->version : 0); //

        if (block.new_producers) {
            values += sep(bulk) + begin_array(bulk);
            for (auto& x : block.new_producers->producers) {
                if (&x != &block.new_producers->producers[0])
                    values += ",";
                values += begin_object_in_array(bulk) + quote(bulk, (std::string)x.producer_name) + "," +
                          quote(bulk, public_key_to_string(x.block_signing_key)) + end_object_in_array(bulk);
            }
            values += end_array(bulk, t, config->schema, "producer_key");
        } else {
            values += sep(bulk) + null_value(bulk);
        }

        write(block_index, t, pipeline, bulk, "block_info", fields, values);
    } // receive_block

    void receive_deltas(uint32_t block_num, input_buffer buf, bool bulk, pqxx::work& t, pqxx::pipeline& pipeline) {
        auto         data = zlib_decompress(buf);
        input_buffer bin{data.data(), data.data() + data.size()};

        auto     num     = read_varuint32(bin);
        unsigned numRows = 0;
        for (uint32_t i = 0; i < num; ++i) {
            check_variant(bin, get_type("table_delta"), "table_delta_v0");
            table_delta_v0 table_delta;
            bin_to_native(table_delta, bin);

            auto& variant_type = get_type(table_delta.name);
            if (!variant_type.filled_variant || variant_type.fields.size() != 1 || !variant_type.fields[0].type->filled_struct)
                throw std::runtime_error("don't know how to proccess " + variant_type.name);
            auto& type = *variant_type.fields[0].type;

            size_t num_processed = 0;
            for (auto& row : table_delta.rows) {
                if (table_delta.rows.size() > 10000 && !(num_processed % 10000))
                    ilog(
                        "block ${b} ${t} ${n} of ${r} bulk=${bulk}",
                        ("b", block_num)("t", table_delta.name)("n", num_processed)("r", table_delta.rows.size())("bulk", bulk));
                check_variant(row.data, variant_type, 0u);
                std::string fields = "block_index, present";
                std::string values = std::to_string(block_num) + sep(bulk) + sql_str(bulk, row.present);
                for (auto& field : type.fields)
                    fill_value(bulk, false, t, "", fields, values, row.data, field);
                write(block_num, t, pipeline, bulk, table_delta.name, fields, values);
                ++num_processed;
            }
            numRows += table_delta.rows.size();
        }
    } // receive_deltas

    void receive_traces(uint32_t block_num, input_buffer buf, bool bulk, pqxx::work& t, pqxx::pipeline& pipeline) {
        auto         data = zlib_decompress(buf);
        input_buffer bin{data.data(), data.data() + data.size()};
        auto         num = read_varuint32(bin);
        for (uint32_t i = 0; i < num; ++i) {
            transaction_trace trace;
            bin_to_native(trace, bin);
            write_transaction_trace(block_num, trace, bulk, t, pipeline);
        }
    }

    void write_transaction_trace(uint32_t block_num, transaction_trace& ttrace, bool bulk, pqxx::work& t, pqxx::pipeline& pipeline) {
        std::string id     = ttrace.failed_dtrx_trace.empty() ? "" : std::string(ttrace.failed_dtrx_trace[0].id);
        std::string fields = "block_index, failed_dtrx_trace";
        std::string values = std::to_string(block_num) + sep(bulk) + quote(bulk, id);
        write("transaction_trace", block_num, ttrace, fields, values, bulk, t, pipeline);

        int32_t num_actions = 0;
        for (auto& atrace : ttrace.action_traces)
            write_action_trace(block_num, ttrace, num_actions, 0, atrace, bulk, t, pipeline);
        if (!ttrace.failed_dtrx_trace.empty()) {
            auto& child = ttrace.failed_dtrx_trace[0];
            write_transaction_trace(block_num, child, bulk, t, pipeline);
        }
    } // write_transaction_trace

    void write_action_trace(
        uint32_t block_num, transaction_trace& ttrace, int32_t& num_actions, int32_t parent_action_index, action_trace& atrace, bool bulk,
        pqxx::work& t, pqxx::pipeline& pipeline) {

        const auto action_index = ++num_actions;

        std::string fields = "block_index, transaction_id, action_index, parent_action_index, transaction_status";
        std::string values = std::to_string(block_num) + sep(bulk) + quote(bulk, (std::string)ttrace.id) + sep(bulk) +
                             std::to_string(action_index) + sep(bulk) + std::to_string(parent_action_index) + sep(bulk) +
                             quote(bulk, to_string(ttrace.status));

        write("action_trace", block_num, atrace, fields, values, bulk, t, pipeline);
        for (auto& child : atrace.inline_traces)
            write_action_trace(block_num, ttrace, num_actions, action_index, child, bulk, t, pipeline);

        write_action_trace_subtable("action_trace_authorization", block_num, ttrace, action_index, atrace.authorization, bulk, t, pipeline);
        write_action_trace_subtable(
            "action_trace_auth_sequence", block_num, ttrace, action_index, atrace.receipt_auth_sequence, bulk, t, pipeline);
        write_action_trace_subtable(
            "action_trace_ram_delta", block_num, ttrace, action_index, atrace.account_ram_deltas, bulk, t, pipeline);
    } // write_action_trace

    template <typename T>
    void write_action_trace_subtable(
        const std::string& name, uint32_t block_num, transaction_trace& ttrace, int32_t action_index, T& objects, bool bulk, pqxx::work& t,
        pqxx::pipeline& pipeline) {

        int32_t num = 0;
        for (auto& obj : objects)
            write_action_trace_subtable(name, block_num, ttrace, action_index, num, obj, bulk, t, pipeline);
    }

    template <typename T>
    void write_action_trace_subtable(
        const std::string& name, uint32_t block_num, transaction_trace& ttrace, int32_t action_index, int32_t& num, T& obj, bool bulk,
        pqxx::work& t, pqxx::pipeline& pipeline) {
        ++num;
        std::string fields = "block_index, transaction_id, action_index, index, transaction_status";
        std::string values = std::to_string(block_num) + sep(bulk) + quote(bulk, (std::string)ttrace.id) + sep(bulk) +
                             std::to_string(action_index) + sep(bulk) + std::to_string(num) + sep(bulk) +
                             quote(bulk, to_string(ttrace.status));

        write(name, block_num, obj, fields, values, bulk, t, pipeline);
    }

    void write(
        uint32_t block_num, pqxx::work& t, pqxx::pipeline& pipeline, bool bulk, const std::string& name, const std::string& fields,
        const std::string& values) {
        if (bulk) {
            write_stream(block_num, t, name, values);
        } else {
            std::string query =
                "insert into " + t.quote_name(config->schema) + "." + t.quote_name(name) + "(" + fields + ") values (" + values + ")";
            pipeline.insert(query);
        }
    }

    template <typename T>
    void write(
        const std::string& name, uint32_t block_num, T& obj, std::string fields, std::string values, bool bulk, pqxx::work& t,
        pqxx::pipeline& pipeline) {

        for_each_field((T*)nullptr, [&](const char* field_name, auto member_ptr) {
            using type              = typename decltype(member_ptr)::member_type;
            constexpr auto sql_type = sql_type_for<type>;
            if constexpr (is_known_type(sql_type)) {
                fields += ", " + t.quote_name(field_name);
                values += sep(bulk) + sql_type.native_to_sql(*sql_connection, bulk, &member_from_void(member_ptr, &obj));
            }
        });
        write(block_num, t, pipeline, bulk, name, fields, values);
    } // write

    void trim() {
        if (!config->enable_trim)
            return;
        auto end_trim = std::min(head, irreversible);
        if (first >= end_trim)
            return;
        create_trim();
        pqxx::work t(*sql_connection);
        ilog("trim  ${b} - ${e}", ("b", first)("e", end_trim));
        t.exec("select * from chain.trim_history(" + std::to_string(first) + ", " + std::to_string(end_trim) + ")");
        t.commit();
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

    ~fpg_session() { ilog("fill-postgresql stopped"); }
}; // fpg_session

static abstract_plugin& _fill_postgresql_plugin = app().register_plugin<fill_pg_plugin>();

fill_postgresql_plugin_impl::~fill_postgresql_plugin_impl() {
    if (session)
        session->my = nullptr;
}

fill_pg_plugin::fill_pg_plugin()
    : my(std::make_shared<fill_postgresql_plugin_impl>()) {}

fill_pg_plugin::~fill_pg_plugin() {}

void fill_pg_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto op   = cfg.add_options();
    auto clop = cli.add_options();
    op("fpg-endpoint", bpo::value<std::string>()->default_value("localhost:8080"), "State-history endpoint to connect to (nodeos)");
    op("fpg-schema", bpo::value<std::string>()->default_value("chain"), "Database schema");
    op("fpg-trim", "Trim history before irreversible");
    clop("fpg-skip-to", bpo::value<uint32_t>(), "Skip blocks before [arg]");
    clop("fpg-stop", bpo::value<uint32_t>(), "Stop before block [arg]");
    clop("fpg-drop", "Drop (delete) schema and tables");
    clop("fpg-create", "Create schema and tables");
}

void fill_pg_plugin::plugin_initialize(const variables_map& options) {
    try {
        auto endpoint             = options.at("fpg-endpoint").as<std::string>();
        auto port                 = endpoint.substr(endpoint.find(':') + 1, endpoint.size());
        auto host                 = endpoint.substr(0, endpoint.find(':'));
        my->config->host          = host;
        my->config->port          = port;
        my->config->schema        = options["fpg-schema"].as<std::string>();
        my->config->skip_to       = options.count("fpg-skip-to") ? options["fpg-skip-to"].as<uint32_t>() : 0;
        my->config->stop_before   = options.count("fpg-stop") ? options["fpg-stop"].as<uint32_t>() : 0;
        my->config->drop_schema   = options.count("fpg-drop");
        my->config->create_schema = options.count("fpg-create");
        my->config->enable_trim   = options.count("fpg-trim");
    }
    FC_LOG_AND_RETHROW()
}

void fill_pg_plugin::plugin_startup() {
    my->session = std::make_shared<fpg_session>(my.get(), app().get_io_service());
    my->session->start();
}

void fill_pg_plugin::plugin_shutdown() {
    if (my->session)
        my->session->close();
}
