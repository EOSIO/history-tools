// copyright defined in LICENSE.txt

// todo: trim: n behind head
// todo: trim: remove last !present

#include "fill_pg_plugin.hpp"
#include "state_history_connection.hpp"
#include "state_history_pg.hpp"
#include "util.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <fc/exception/exception.hpp>
#include <boost/algorithm/string.hpp> 
#include <pqxx/tablewriter>

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

    table_stream(const std::string& db_str,const std::string& name, std::vector<std::string>& cols)
        :c(db_str) 
        ,t(c)
        , writer(t, name,cols.begin(),cols.end()) {}
};

struct fpg_session;

struct fill_postgresql_config : connection_config {
    std::string             schema;
    std::string             dbstring;
    uint32_t                skip_to       = 0;
    uint32_t                stop_before   = 0;
    std::vector<trx_filter> trx_filters   = {};
    bool                    drop_schema   = false;
    bool                    create_schema = false;
    bool                    enable_trim   = false;
    bool                    ignore_on_block = false;
    bool                    remove_old_delta_row = false;
};

struct fill_postgresql_plugin_impl : std::enable_shared_from_this<fill_postgresql_plugin_impl> {
    std::shared_ptr<fill_postgresql_config> config = std::make_shared<fill_postgresql_config>();
    std::shared_ptr<fpg_session>            session;
    boost::asio::deadline_timer             timer;

    fill_postgresql_plugin_impl()
        : timer(app().get_io_service()) {}

    ~fill_postgresql_plugin_impl();

    void schedule_retry() {
        timer.expires_from_now(boost::posix_time::seconds(1));
        timer.async_wait([this](auto&) {
            ilog("retry...");
            start();
        });
    }

    void start();
};

struct fpg_session : connection_callbacks, std::enable_shared_from_this<fpg_session> {
    fill_postgresql_plugin_impl*                         my = nullptr;
    std::shared_ptr<fill_postgresql_config>              config;
    std::optional<pqxx::connection>                      sql_connection;
    std::shared_ptr<state_history::connection>           connection;
    bool                                                 created_trim    = false;
    uint32_t                                             head            = 0;
    std::string                                          head_id         = "";
    uint32_t                                             irreversible    = 0;
    std::string                                          irreversible_id = "";
    uint32_t                                             first           = 0;
    uint32_t                                             first_bulk      = 0;
    std::map<std::string, std::unique_ptr<table_stream>> table_streams;
    std::map<std::string, std::vector<std::string>>      abi_table_keys; // table name -> primary keys
    std::set<std::string>                                prepared_query;
    fpg_session(fill_postgresql_plugin_impl* my)
        : my(my)
        , config(my->config) {
        ilog("connect to postgresql");
        sql_connection.emplace(config->dbstring);
    }

    void start(asio::io_context& ioc) {
        if (config->drop_schema) {
            pqxx::work t(*sql_connection);
            ilog("drop schema ${s}", ("s", t.quote_name(config->schema)));
            t.exec("drop schema if exists " + t.quote_name(config->schema) + " cascade");
            t.commit();
            config->drop_schema = false;
        }

        connection = std::make_shared<state_history::connection>(ioc, *config, shared_from_this());
        connection->connect();
    }

    void received_abi(std::string_view abi) override {
        if (config->create_schema) {
            create_tables();
            config->create_schema = false;
        }
        connection->send(get_status_request_v0{});
    }

    bool received(get_status_result_v0& status) override {
        pqxx::work t(*sql_connection);
        load_fill_status(t);
        auto           positions = get_positions(t);
        pqxx::pipeline pipeline(t);
        truncate(t, pipeline, head + 1);
        pipeline.complete();
        t.commit();
        std::cout << "get_status_result_v0:" << status.head.block_num <<  "\t" << status.last_irreversible.block_num << std::endl;
        connection->request_blocks(status, std::max(config->skip_to, head + 1), positions);
        return true;
    }

    template <typename T>
    void add_table_field(pqxx::work& t, std::string& fields, const std::string& field_name) {
        if constexpr (is_known_type(type_for<T>)) {
            std::string type_name = type_for<T>.name;
            if (type_name == "transaction_status_type")
                type_name = t.quote_name(config->schema) + "." + type_name;
            fields += ", "s + t.quote_name(field_name) + " " + type_name;
        } else if constexpr (abieos::is_optional_v<T>) {
            fields += ", "s + t.quote_name(field_name + "_present") + " boolean";
            add_table_field<typename T::value_type>(t, fields, field_name);
        } else if constexpr (abieos::is_variant_v<T>) {
            add_table_fields<std::variant_alternative_t<0, T>>(t, fields, field_name + "_");
        } else if constexpr (abieos::is_vector_v<T>) {
        } else {
            add_table_fields<T>(t, fields, field_name + "_");
        }
    }

    template <typename T>
    void    add_table_fields(pqxx::work& t, std::string& fields, const std::string& prefix) {
        for_each_field((T*)nullptr, [&](const char* field_name, auto member_ptr) {
            add_table_field<typename decltype(member_ptr)::member_type>(t, fields, prefix + field_name);
        });
    }

    template <typename T>
    void create_table( //
        pqxx::work& t, const std::string& name, const std::string& pk, std::string fields, const char* suffix_fields = nullptr) {

        add_table_fields<T>(t, fields, "");
        if (suffix_fields)
            fields += ","s + suffix_fields;
        std::string query =
            "create table " + t.quote_name(config->schema) + "." + t.quote_name(name) + "(" + fields + ", primary key (" + pk + "))";
        t.exec(query);
    }

    void fill_field(pqxx::work& t, const std::string& base_name, std::string& fields, const abi_field& field) {
        if (field.type->filled_struct) {
            for (auto& f : field.type->fields)
                fill_field(t, base_name + field.name + "_", fields, f);
        } else if (field.type->optional_of && field.type->optional_of->filled_struct) {
            fields += ", "s + t.quote_name(base_name + field.name + "_present") + " boolean";
            for (auto& f : field.type->optional_of->fields)
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
        } else if (field.type->array_of && field.type->array_of->filled_variant && field.type->array_of->fields[0].type->filled_struct) {
            auto*       s = field.type->array_of->fields[0].type;
            std::string sub_fields;
            for (auto& f : s->fields)
                fill_field(t, "", sub_fields, f);
            std::string query =
                "create type " + t.quote_name(config->schema) + "." + t.quote_name(s->name) + " as (" + sub_fields.substr(2) + ")";
            t.exec(query);
            fields += ", " + t.quote_name(base_name + field.name) + " " + t.quote_name(config->schema) + "." + t.quote_name(s->name) + "[]";
        } else {
            auto abi_type = field.type->name;
            if (abi_type.size() >= 1 && abi_type.back() == '?')
                abi_type.resize(abi_type.size() - 1);
            auto it = abi_type_to_sql_type.find(abi_type);
            if (it == abi_type_to_sql_type.end())
                throw std::runtime_error("don't know sql type for abi type: " + abi_type);
            std::string type = it->second.name;
            if (type == "transaction_status_type")
                type = t.quote_name(config->schema) + "." + type;
            fields += ", " + t.quote_name(base_name + field.name) + " " + it->second.name;
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
            R"(.received_block ("block_num" bigint, "block_id" varchar(64), primary key("block_num")))");
        t.exec(
            "create table " + t.quote_name(config->schema) +
            R"(.received_nonempty_block ("block_num" bigint, "block_id" varchar(64), "transaction_count" integer, "timestamp" timestamp, primary key("block_num")))");
        t.exec(
            "create table " + t.quote_name(config->schema) +
            R"(.fill_status ("head" bigint, "head_id" varchar(64), "irreversible" bigint, "irreversible_id" varchar(64), "first" bigint))");
        t.exec("create unique index on " + t.quote_name(config->schema) + R"(.fill_status ((true)))");
        t.exec("insert into " + t.quote_name(config->schema) + R"(.fill_status values (0, '', 0, '', 0))");

        // clang-format off
        create_table<permission_level>(         t, "action_trace_authorization",  "block_num, transaction_id, action_ordinal, ordinal", "block_num bigint, transaction_id varchar(64), action_ordinal integer, ordinal integer, transaction_status " + t.quote_name(config->schema) + ".transaction_status_type");
        create_table<account_auth_sequence>(    t, "action_trace_auth_sequence",  "block_num, transaction_id, action_ordinal, ordinal", "block_num bigint, transaction_id varchar(64), action_ordinal integer, ordinal integer, transaction_status " + t.quote_name(config->schema) + ".transaction_status_type");
        create_table<account_delta>(            t, "action_trace_ram_delta",      "block_num, transaction_id, action_ordinal, ordinal", "block_num bigint, transaction_id varchar(64), action_ordinal integer, ordinal integer, transaction_status " + t.quote_name(config->schema) + ".transaction_status_type");


        create_table<action_trace_v0>(          t, "action_trace",                "block_num, transaction_id, action_ordinal",          "block_num bigint, \"timestamp\" timestamp, transaction_id varchar(64), transaction_status " + t.quote_name(config->schema) + ".transaction_status_type, actor varchar(13), permission varchar(13), token_from varchar(13), token_to varchar(13), amount bigint, symbol varchar(7)");

        // add supplimentary indexes to action_trace table
        t.exec("create index act_account_index on " + t.quote_name(config->schema) + ".action_trace (act_account, receipt_global_sequence)");
        t.exec("create index actor_index on " + t.quote_name(config->schema) + ".action_trace (actor, receipt_global_sequence)");
        t.exec("create index from_index on " + t.quote_name(config->schema) + ".action_trace (token_from, receipt_global_sequence)");
        t.exec("create index to_index on " + t.quote_name(config->schema) + ".action_trace (token_to, receipt_global_sequence)");

        create_table<transaction_trace_v0>(     t, "transaction_trace",           "block_num, transaction_ordinal",                     "block_num bigint, transaction_ordinal integer, failed_dtrx_trace varchar(64)", "partial_signatures varchar[], partial_context_free_data bytea[], seq_id serial");

        t.exec("create index transaction_id_index on " + t.quote_name(config->schema) + ".transaction_trace (id, block_num)");

        // clang-format on

        for (auto& table : connection->abi.tables) {
            if (table.type == "global_property")
                continue;
            auto& variant_type = get_type(table.type);
            if (!variant_type.filled_variant || variant_type.fields.size() != 1 || !variant_type.fields[0].type->filled_struct)
                throw std::runtime_error("don't know how to proccess " + variant_type.name);
            auto&       type   = *variant_type.fields[0].type;
            std::string fields = "block_num bigint, present bool";
            for (auto& field : type.fields)
                fill_field(t, "", fields, field);
            std::string other_keys = "block_num, present", keys;
            for (auto& key : table.key_names)
                keys += (keys.length() ? ", " : "") + t.quote_name(key);
            std::string query =
                "create table " + t.quote_name(config->schema) + "." + table.type + "(" + fields + ", primary key(" + (other_keys + (keys.length() ? "," : "") + keys) + "))";
            t.exec(query);

            if (keys.length()) {
               query = "create index " + config->schema + "_" + table.type + "_index on " +
                  t.quote_name(config->schema) + "." + table.type +" (" + keys + ")";
               t.exec(query);
            }
        }

        for (auto& table : connection->abi.tables) {
            abi_table_keys[table.type].resize(0);
            for (auto& key : table.key_names) {
                abi_table_keys[table.type].push_back(key);
            }
		}

        t.exec(
            "create table " + t.quote_name(config->schema) +
            R"(.block_info(
                "block_num" bigint,
                "block_id" varchar(64),
                "timestamp" timestamp,
                "producer" varchar(13),
                "confirmed" integer,
                "previous" varchar(64),
                "transaction_count" integer,
                "transaction_mroot" varchar(64),
                "action_mroot" varchar(64),
                "schedule_version" bigint,
                "new_producers_version" bigint,
                primary key("block_num")))");

        t.commit();
    } // create_tables()

    void create_trim() {
        if (created_trim)
            return;
        pqxx::work t(*sql_connection);
        ilog("create_trim");
        for (auto& table : connection->abi.tables) {
            if (table.type == "global_property")
                continue;
            if (table.key_names.empty())
                continue;
            std::string query = "create index if not exists " + table.type;
            for (auto& k : table.key_names)
                query += "_" + k;
            query += "_block_present_idx on " + t.quote_name(config->schema) + "." + t.quote_name(table.type) + "(\n";
            for (auto& k : table.key_names)
                query += "    " + t.quote_name(k) + ",\n";
            query += "    \"block_num\" desc,\n    \"present\" desc\n)";
            // std::cout << query << ";\n\n";
            t.exec(query);
        }

        std::string query = R"(
            drop function if exists )" +
                            t.quote_name(config->schema) + R"(.trim_history;
        )";
        // std::cout << query << "\n";
        t.exec(query);

        query = R"(
            create function )" +
                t.quote_name(config->schema) + R"(.trim_history(
                prev_block_num bigint,
                irrev_block_num bigint
            ) returns void
            as $$
                declare
                    key_search record;
                begin)";

        static const char* const simple_cases[] = {
            "received_block",
            "received_nonempty_block",
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
                        block_num >= prev_block_num
                        and block_num < irrev_block_num;
                    )";
        }

        auto add_trim = [&](const std::string& table, const std::string& keys, const std::string& search_keys) {
            query += R"(
                    for key_search in
                        select
                            distinct on()" +
                     keys + R"()
                            )" +
                     keys + R"(, block_num
                        from
                            )" +
                     t.quote_name(config->schema) + "." + t.quote_name(table) + R"(
                        where
                            block_num > prev_block_num and block_num <= irrev_block_num
                        order by )" +
                     keys + R"(, block_num desc, present desc
                    loop
                        delete from )" +
                     t.quote_name(config->schema) + "." + t.quote_name(table) + R"(
                        where
                            ()" +
                     keys + R"() = ()" + search_keys + R"()
                            and block_num < key_search.block_num;
                    end loop;
                    )";
        };

        for (auto& table : connection->abi.tables) {
            if (table.type == "global_property")
                continue;
            if (table.key_names.empty()) {
                query += R"(
                    for key_search in
                        select
                            block_num
                        from
                            )" +
                         t.quote_name(config->schema) + "." + t.quote_name(table.type) + R"(
                        where
                            block_num > prev_block_num and block_num <= irrev_block_num
                        order by block_num desc, present desc
                        limit 1
                    loop
                        delete from )" +
                         t.quote_name(config->schema) + "." + t.quote_name(table.type) + R"(
                        where
                            block_num < key_search.block_num;
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
                add_trim(table.type, keys, search_keys);
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

    std::vector<block_position> get_positions(pqxx::work& t) {
        std::vector<block_position> result;
        auto                        rows = t.exec(
            "select block_num, block_id from " + t.quote_name(config->schema) + ".received_block where block_num >= " +
            std::to_string(irreversible) + " and block_num <= " + std::to_string(head) + " order by block_num");
        for (auto row : rows)
            result.push_back({row[0].as<uint32_t>(), sql_to_checksum256(row[1].as<std::string>().c_str())});
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
                "delete from " + t.quote_name(config->schema) + "." + t.quote_name(name) + " where block_num >= " + std::to_string(block));
        };
        trunc("received_block");
        trunc("received_nonempty_block");
        trunc("action_trace_authorization");
        trunc("action_trace_auth_sequence");
        trunc("action_trace_ram_delta");
        trunc("action_trace");
        trunc("transaction_trace");
        for (auto& table : connection->abi.tables) {
            if (table.type == "global_property")
                continue;
            trunc(table.type);
        }

        auto result = pipeline.retrieve(pipeline.insert(
            "select block_id from " + t.quote_name(config->schema) + ".received_block where block_num=" + std::to_string(block - 1)));
        if (result.empty()) {
            head    = 0;
            head_id = "";
        } else {
            head    = block - 1;
            head_id = result.front()[0].as<std::string>();
        }
        first = std::min(first, head);
    } // truncate

    bool received(get_blocks_result_v0& result) override {
        if (!result.this_block)
            return true;
        bool bulk         = result.this_block->block_num + 4 < result.last_irreversible.block_num;
        // std::cout << "bulk:" << result.this_block->block_num << "\t" << result.last_irreversible.block_num << std::endl;
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

		// if (config->remove_old_delta_row) {
		// 	bulk = false;
		// }

        if (!bulk || large_deltas || !(result.this_block->block_num % 200))
            close_streams();
        if (table_streams.empty())
            trim();
        if (!bulk)
            ilog("block ${b}", ("b", result.this_block->block_num));

        pqxx::work     t(*sql_connection);
        pqxx::pipeline pipeline(t);
        auto start_time = boost::posix_time::microsec_clock::local_time();
        if (result.this_block->block_num <= head)
            truncate(t, pipeline, result.this_block->block_num);
        if (!head_id.empty() && (!result.prev_block || (std::string)result.prev_block->block_id != head_id))
            throw std::runtime_error("prev_block does not match");

        signed_block block;
        if (result.block)
            receive_block(result.this_block->block_num, result.this_block->block_id, *result.block, bulk, t, pipeline, block);
        if (result.deltas)
            receive_deltas(result.this_block->block_num, *result.deltas, false, t, pipeline);

        if (result.traces)
            receive_traces(result.this_block->block_num, block.timestamp, *result.traces, bulk, t, pipeline);

        head            = result.this_block->block_num;
        head_id         = (std::string)result.this_block->block_id;
        irreversible    = result.last_irreversible.block_num;
        irreversible_id = (std::string)result.last_irreversible.block_id;
        if (!first)
            first = head;
        if (!bulk)
            write_fill_status(t, pipeline);
        pipeline.insert(
            "insert into " + t.quote_name(config->schema) + ".received_block (block_num, block_id) values (" +
            std::to_string(result.this_block->block_num) + ", " + quote(std::string(result.this_block->block_id)) + ")");

        if (result.block && block.transactions.size() > 0) {
            pipeline.insert(
                "insert into " + t.quote_name(config->schema) + ".received_nonempty_block (block_num, block_id, transaction_count, timestamp) values (" +
                std::to_string(result.this_block->block_num) + ", " +
                quote(std::string(result.this_block->block_id)) + ", " +
                std::to_string(block.transactions.size()) + ", " +
                quote(std::string(block.timestamp)) + ")");
        }

        pipeline.complete();
        t.commit();
        auto finish_time = boost::posix_time::microsec_clock::local_time();
        std::cout << "benchmark:" << (finish_time-start_time).total_milliseconds() <<"," << result.this_block->block_num << "," << block.transactions.size() << std::endl;
        if (large_deltas)
            close_streams();
        return true;
    } // receive_result()

    void write_stream(uint32_t block_num, pqxx::work& t, const std::string& name, const std::string& fields, const std::string& values) {
        std::cout << "write with stream:" << block_num << std::endl;
        if (!first_bulk)
            first_bulk = block_num;
        auto& ts = table_streams[name+fields];
        if (!ts){

            std::vector<std::string> cols;
            boost::split(cols,fields,boost::is_any_of(","));
            ts = std::make_unique<table_stream>(config->dbstring,t.quote_name(config->schema) + "." + t.quote_name(name),cols);
        }
        std::cout << name << ":" << values << std::endl;
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
        input_buffer& bin, const abi_field& field, std::string *delete_query = nullptr) {
        if (field.type->filled_struct) {
            for (auto& f : field.type->fields)
                fill_value(bulk, nested_bulk, t, base_name + field.name + "_", fields, values, bin, f);
        } else if (field.type->optional_of && field.type->optional_of->filled_struct) {
            auto present = read_raw<bool>(bin);
            fields += ", " + t.quote_name(base_name + field.name + "_present");
            values += sep(bulk) + sql_str(bulk, present);
            if (present) {
                for (auto& f : field.type->optional_of->fields)
                    fill_value(bulk, nested_bulk, t, base_name + field.name + "_", fields, values, bin, f);
            } else {
                for (auto& f : field.type->optional_of->fields) {
                    auto it = abi_type_to_sql_type.find(f.type->name);
                    if (it == abi_type_to_sql_type.end())
                        throw std::runtime_error("don't know sql type for abi type: " + f.type->name);
                    if (!it->second.empty_to_sql)
                        throw std::runtime_error("don't know how to process empty " + field.type->name);
                    fields += ", " + t.quote_name(base_name + field.name + "_" + f.name);
                    values += sep(bulk) + it->second.empty_to_sql(*sql_connection, bulk);
                }
            }
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
        } else if (field.type->array_of && field.type->array_of->filled_variant && field.type->array_of->fields[0].type->filled_struct) {
            auto* s = field.type->array_of->fields[0].type;
            fields += ", " + t.quote_name(base_name + field.name);
            values += sep(bulk) + begin_array(bulk);
            uint32_t n = read_varuint32(bin);
            for (uint32_t i = 0; i < n; ++i) {
                if (read_varuint32(bin) != 0)
                    throw std::runtime_error("expected 0 variant index");
                if (i)
                    values += ",";
                values += begin_object_in_array(bulk);
                std::string struct_fields;
                std::string struct_values;
                for (auto& f : s->fields)
                    fill_value(bulk, true, t, "", struct_fields, struct_values, bin, f);
                if (bulk)
                    values += struct_values.substr(1);
                else
                    values += struct_values.substr(2);
                values += end_object_in_array(bulk);
            }
            values += end_array(bulk, t, config->schema, s->name);
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
			std::string val;
            if (bulk) {
                if (nested_bulk)
                    values += ",";
                else
                    values += "\t";
                if (!is_optional || read_raw<bool>(bin)) {
					val = it->second.bin_to_sql(*sql_connection, bulk, bin);
                    values += val;
				} else
                    values += "\\N";
            } else {
                if (!is_optional || read_raw<bool>(bin)) {
					val = it->second.bin_to_sql(*sql_connection, bulk, bin);
                    values += ", " + val;
				} else
                    values += ", null";
            }
			if (delete_query && val.length()) {
				if (delete_query->length()) {
					*delete_query += " and ";
				}
				*delete_query += t.quote_name(base_name + field.name) + " = " + val;
			}
        }
    } // fill_value

    void
    receive_block(uint32_t block_num, const checksum256& block_id, input_buffer bin, bool bulk, pqxx::work& t, pqxx::pipeline& pipeline, signed_block &block) {
        bin_to_native(block, bin);

        std::string fields = "block_num, block_id, timestamp, producer, confirmed, , transaction_count, transaction_mroot, action_mroot, "
                             "schedule_version, new_producers_version"; // , new_producers";
        std::string values = sql_str(bulk, block_num) + sep(bulk) +                                 //
                             sql_str(bulk, block_id) + sep(bulk) +                                  //
                             sql_str(bulk, block.timestamp) + sep(bulk) +                           //
                             sql_str(bulk, block.producer) + sep(bulk) +                            //
                             sql_str(bulk, block.confirmed) + sep(bulk) +                           //
                             sql_str(bulk, block.previous) + sep(bulk) +                            //
                             sql_str(bulk, (uint32_t)(block.transactions.size())) + sep(bulk) +     //
                             sql_str(bulk, block.transaction_mroot) + sep(bulk) +                   //
                             sql_str(bulk, block.action_mroot) + sep(bulk) +                        //
                             sql_str(bulk, block.schedule_version) + sep(bulk) +                    //
                             sql_str(bulk, block.new_producers ? block.new_producers->version : 0); //

        /*
        if (block.new_producers) {
            values += sep(bulk) + begin_array(bulk);
            for (auto& x : block.new_producers->producers) {
                if (&x != &block.new_producers->producers[0])
                    values += ",";
                values += begin_object_in_array(bulk) + quote(bulk, (std::string)x.producer_name) + "," +
                          quote(bulk, public_key_to_string_EOS(x.block_signing_key)) + end_object_in_array(bulk);
            }
            values += end_array(bulk, t, config->schema, "producer_key");
        } else {
            values += sep(bulk) + null_value(bulk);
        }
        */

        write(block_num, t, pipeline, bulk, "block_info", fields, values);
    } // receive_block

    void receive_deltas(uint32_t block_num, input_buffer bin, bool bulk, pqxx::work& t, pqxx::pipeline& pipeline) {
        auto     num     = read_varuint32(bin);
        unsigned numRows = 0;
        for (uint32_t i = 0; i < num; ++i) {
            check_variant(bin, get_type("table_delta"), "table_delta_v0");
            table_delta_v0 table_delta;
            bin_to_native(table_delta, bin);

            if (table_delta.name == "global_property")
                continue;

            auto& variant_type = get_type(table_delta.name);
            if (!variant_type.filled_variant || variant_type.fields.size() != 1 || !variant_type.fields[0].type->filled_struct)
                throw std::runtime_error("don't know how to proccess " + variant_type.name);
            auto& type = *variant_type.fields[0].type;

			auto& primary_keys = abi_table_keys[table_delta.name];


            std::cout << "table delta name::" << table_delta.name << std::endl;
            size_t num_processed = 0;
            for (auto& row : table_delta.rows) {
                if (table_delta.rows.size() > 10000 && !(num_processed % 10000))
                    ilog(
                        "block ${b} ${t} ${n} of ${r} bulk=${bulk}",
                        ("b", block_num)("t", table_delta.name)("n", num_processed)("r", table_delta.rows.size())("bulk", bulk));
                check_variant(row.data, variant_type, 0u);
                std::string fields = "block_num, present";
                std::string values = std::to_string(block_num) + sep(bulk) + sql_str(bulk, row.present);
				std::string delete_query;
                for (auto& field : type.fields) {
                    std::cout << "field:" << field.name << std::endl;
                    bool is_key_field = false;
					if (config->remove_old_delta_row ) {
						for (auto& key : primary_keys) {
							if (key == field.name) {
								is_key_field = true;
								break;
							}
						}
					}
                    fill_value(bulk, false, t, "", fields, values, row.data, field, (is_key_field ? &delete_query : nullptr));
                }
				if (config->remove_old_delta_row && !bulk && delete_query.length()) {
					delete_query = "delete from " + t.quote_name(config->schema) + "." + table_delta.name + " where " + delete_query;
                    std::cout << delete_query << std::endl;
                    std::cout << "the insert after:" << fields << "----" << values << std::endl;
					pipeline.insert(std::move(delete_query));
                    // t.exec(delete_query);
				}
                write(block_num, t, pipeline, bulk, table_delta.name, fields, values);
                ++num_processed;
            }
            numRows += table_delta.rows.size();
        }
    } // receive_deltas

    void receive_traces(uint32_t block_num, abieos::block_timestamp timestamp, input_buffer bin, bool bulk, pqxx::work& t, pqxx::pipeline& pipeline) {
        uint32_t num          = read_varuint32(bin);
        uint32_t num_ordinals = 0;
        for (uint32_t i = 0; i < num; ++i) {
            transaction_trace trace;
            bin_to_native(trace, bin);
            if (i == 0 && config->ignore_on_block) {
               ++num_ordinals;
               continue;
            } else if (filter(config->trx_filters, std::get<0>(trace))) {
               write_transaction_trace(block_num, timestamp, num_ordinals, std::get<transaction_trace_v0>(trace), bulk, t, pipeline);
            }
        }
    }

    void write_transaction_trace(
        uint32_t block_num, abieos::block_timestamp timestamp, uint32_t& num_ordinals, transaction_trace_v0& ttrace, bool bulk, pqxx::work& t, pqxx::pipeline& pipeline) {
        auto* failed = !ttrace.failed_dtrx_trace.empty() ? &std::get<transaction_trace_v0>(ttrace.failed_dtrx_trace[0].recurse) : nullptr;
        if (failed) {
            if (!filter(config->trx_filters, *failed))
                return;
            write_transaction_trace(block_num, timestamp, num_ordinals, *failed, bulk, t, pipeline);
        }
        auto        transaction_ordinal = ++num_ordinals;
        std::string failed_id           = failed ? std::string(failed->id) : "";
        std::string fields              = "block_num, transaction_ordinal, failed_dtrx_trace";
        std::string values =
            std::to_string(block_num) + sep(bulk) + std::to_string(transaction_ordinal) + sep(bulk) + quote(bulk, failed_id);
        std::string suffix_fields = ", partial_signatures, partial_context_free_data";
        std::string suffix_values = sep(bulk) + begin_array(bulk);
        if (ttrace.partial) {
            auto& partial = std::get<partial_transaction_v0>(*ttrace.partial);
            for (auto& sig : partial.signatures) {
                if (&sig != &partial.signatures[0])
                    suffix_values += ",";
                suffix_values += native_to_sql<abieos::signature>(*sql_connection, bulk, &sig);
            }
        }
        suffix_values += end_array(bulk, "varchar") + sep(bulk) + begin_array(bulk);
        if (ttrace.partial) {
            auto& partial = std::get<partial_transaction_v0>(*ttrace.partial);
            for (auto& cfd : partial.context_free_data) {
                if (&cfd != &partial.context_free_data[0])
                    suffix_values += ",";
                suffix_values += native_to_sql<abieos::input_buffer>(*sql_connection, bulk, &cfd);
            }
        }
        suffix_values += end_array(bulk, "bytea");
        write(
            "transaction_trace", block_num, ttrace, std::move(fields), std::move(values), bulk, t, pipeline, std::move(suffix_fields),
            std::move(suffix_values));

        for (auto& atrace : ttrace.action_traces)
            write_action_trace(block_num, timestamp, ttrace, std::get<action_trace_v0>(atrace), bulk, t, pipeline);
    } // write_transaction_trace

    void write_action_trace(
        uint32_t block_num, abieos::block_timestamp timestamp, transaction_trace_v0& ttrace, action_trace_v0& atrace, bool bulk, pqxx::work& t, pqxx::pipeline& pipeline) {

        std::string fields = "block_num, timestamp, transaction_id, transaction_status";
        std::string values =
            std::to_string(block_num) + sep(bulk) + sql_str(bulk, timestamp) + sep(bulk) + quote(bulk, (std::string)ttrace.id) + sep(bulk) + quote(bulk, to_string(ttrace.status));

        // additional (business) fields
        if (atrace.act.authorization.size()) {
            fields += ", actor, permission";
            values += sep(bulk) + quote(bulk, std::string(atrace.act.authorization[0].actor)) + sep(bulk) + quote(bulk, std::string(atrace.act.authorization[0].permission));
        }
        if (atrace.act.name == abieos::name("transfer") && atrace.act.account == abieos::name("eosio.token")) {
            abieos::input_buffer buffer = atrace.act.data; // need copy of input_buffer
            uint64_t from = 0, to = 0, symbol = 0, amount = 0;
            std::string error;
            if (read_raw(buffer, error, from) &&
                read_raw(buffer, error, to) &&
                read_raw(buffer, error, amount) &&
                read_raw(buffer, error, symbol)) {
                fields += ", token_from, token_to, symbol, amount";
                values += sep(bulk) + quote(bulk, name_to_string(from)) + sep(bulk) + quote(bulk, name_to_string(to)) + sep(bulk) + quote(bulk, symbol_code_to_string(symbol >> 8)) + sep(bulk) + std::to_string(amount);
            }
        }

        write("action_trace", block_num, atrace, fields, values, bulk, t, pipeline);
        write_action_trace_subtable(
            "action_trace_authorization", block_num, ttrace, atrace.action_ordinal.value, atrace.act.authorization, bulk, t, pipeline);
        if (atrace.receipt)
            write_action_trace_subtable(
                "action_trace_auth_sequence", block_num, ttrace, atrace.action_ordinal.value,
                std::get<action_receipt_v0>(*atrace.receipt).auth_sequence, bulk, t, pipeline);
        write_action_trace_subtable(
            "action_trace_ram_delta", block_num, ttrace, atrace.action_ordinal.value, atrace.account_ram_deltas, bulk, t, pipeline);
    } // write_action_trace

    template <typename T>
    void write_action_trace_subtable(
        const std::string& name, uint32_t block_num, transaction_trace_v0& ttrace, int32_t action_ordinal, T& objects, bool bulk,
        pqxx::work& t, pqxx::pipeline& pipeline) {

        int32_t num = 0;
        for (auto& obj : objects)
            write_action_trace_subtable(name, block_num, ttrace, action_ordinal, num, obj, bulk, t, pipeline);
    }

    template <typename T>
    void write_action_trace_subtable(
        const std::string& name, uint32_t block_num, transaction_trace_v0& ttrace, int32_t action_ordinal, int32_t& num, T& obj, bool bulk,
        pqxx::work& t, pqxx::pipeline& pipeline) {
        ++num;

        std::string fields = "block_num, transaction_id, action_ordinal, ordinal, transaction_status";
        std::string values = std::to_string(block_num) + sep(bulk) + quote(bulk, (std::string)ttrace.id) + sep(bulk) +
                             std::to_string(action_ordinal) + sep(bulk) + std::to_string(num) + sep(bulk) +
                             quote(bulk, to_string(ttrace.status));

        write(name, block_num, obj, fields, values, bulk, t, pipeline);
    }


    void write(
        uint32_t block_num, pqxx::work& t, pqxx::pipeline& pipeline, bool bulk, const std::string& name, const std::string& fields,
        const std::string& values) {
        if (bulk) {
            // std::cout << "write bulk with stream." << std::endl;
            write_stream(block_num, t, name,fields, values);
        } else {
            // std::cout << "no bulk!!!!!" << std::endl;
            // if(prepared_query.find(t.quote_name(config->schema) + "." + t.quote_name(name)+std::to_string(std::hash<std::string>{}(fields))) == prepared_query.end()){

            //     int n = std::count(fields.begin(), fields.end(), ',');
            //     std::string pls;
            //     for(int i = 0;i<n;i++){
            //         pls+='$';
            //         pls+=std::to_string(i+1);
            //         pls+=",";
            //     }
            //     pls+='$';
            //     pls+=std::to_string(n+1);


            //     std::string prepare = 
            //         "prepare insert_"+config->schema + "_" + name +"_" + std::to_string(std::hash<std::string>{}(fields)) + " AS insert into " + config->schema + "." + name + " (" + fields + ") values (" + pls + ")";

            //     pipeline.insert(prepare);
            //     prepared_query.insert(t.quote_name(config->schema) + "." + t.quote_name(name)+ std::to_string(std::hash<std::string>{}(fields)));
            //     // std::cout << prepare <<std::endl;
            // }


            // std::string query =
            //     "execute insert_" + config->schema + "_" + name+"_" + std::to_string(std::hash<std::string>{}(fields))+"(" + values + ")";
            // pipeline.insert(query);
            // std::cout << query <<std::endl;

            std::string query =
                "insert into " + t.quote_name(config->schema) + "." + t.quote_name(name) + "(" + fields + ") values (" + values + ")";
            pipeline.insert(query);

        }
    }

    template <typename T>
    void write_table_field(
        const T& obj, std::string& fields, std::string& values, const std::string& field_name, bool bulk, pqxx::work& t,
        pqxx::pipeline& pipeline) {
        if constexpr (is_known_type(type_for<T>)) {
            fields += ", " + t.quote_name(field_name);
            values += sep(bulk) + type_for<T>.native_to_sql(*sql_connection, bulk, &obj);
        } else if constexpr (abieos::is_optional_v<T>) {
            fields += ", "s + t.quote_name(field_name + "_present");
            bool hv = obj.has_value();
            values += sep(bulk) + type_for<bool>.native_to_sql(*sql_connection, bulk, &hv);
            write_table_field(obj ? *obj : typename T::value_type{}, fields, values, field_name, bulk, t, pipeline);
        } else if constexpr (abieos::is_variant_v<T>) {
            write_table_fields(std::get<0>(obj), fields, values, field_name + "_", bulk, t, pipeline);
        } else if constexpr (abieos::is_vector_v<T>) {
        } else {
            write_table_fields<T>(obj, fields, values, field_name + "_", bulk, t, pipeline);
        }
    }

    template <typename T>
    void write_table_fields(
        const T& obj, std::string& fields, std::string& values, const std::string& prefix, bool bulk, pqxx::work& t,
        pqxx::pipeline& pipeline) {
        for_each_field((T*)nullptr, [&](const char* field_name, auto member_ptr) {
            write_table_field(member_from_void(member_ptr, &obj), fields, values, prefix + field_name, bulk, t, pipeline);
        });
    }

    template <typename T>
    void write(
        const std::string& name, uint32_t block_num, T& obj, std::string fields, std::string values, bool bulk, pqxx::work& t,
        pqxx::pipeline& pipeline, std::string suffix_fields = "", std::string suffix_values = "") {

        write_table_fields(obj, fields, values, "", bulk, t, pipeline);
        fields += suffix_fields;
        values += suffix_values;
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
        t.exec(
            "select * from " + t.quote_name(config->schema) + ".trim_history(" + std::to_string(first) + ", " + std::to_string(end_trim) +
            ")");
        t.commit();
        ilog("      done");
        first = end_trim;
    }

    const abi_type& get_type(const std::string& name) { return connection->get_type(name); }

    void closed(bool retry) override {
        if (my) {
            my->session.reset();
            if (retry)
                my->schedule_retry();
        }
    }

    ~fpg_session() {}
}; // fpg_session

static abstract_plugin& _fill_postgresql_plugin = app().register_plugin<fill_pg_plugin>();

fill_postgresql_plugin_impl::~fill_postgresql_plugin_impl() {
    if (session)
        session->my = nullptr;
}

void fill_postgresql_plugin_impl::start() {
    session = std::make_shared<fpg_session>(this);
    session->start(app().get_io_service());
}

fill_pg_plugin::fill_pg_plugin()
    : my(std::make_shared<fill_postgresql_plugin_impl>()) {}

fill_pg_plugin::~fill_pg_plugin() {}

void fill_pg_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto clop = cli.add_options();
    clop("fpg-drop", "Drop (delete) schema and tables");
    clop("fpg-create", "Create schema and tables");
    clop("ignore-onblock", "Ignore onblock transactions");
    clop("remove_old_delta_row", "remove old row of the same primary key from abi delta tables before every insertion, performance will degrade if enabled");
    clop("dbstring", bpo::value<std::string>(), "dbstring of postgresql");
}

void fill_pg_plugin::plugin_initialize(const variables_map& options) {
    try {
        auto endpoint = options.at("fill-connect-to").as<std::string>();
        if (endpoint.find(':') == std::string::npos)
            throw std::runtime_error("invalid endpoint: " + endpoint);

        auto port                 = endpoint.substr(endpoint.find(':') + 1, endpoint.size());
        auto host                 = endpoint.substr(0, endpoint.find(':'));
        my->config->host          = host;
        my->config->port          = port;
        my->config->dbstring      = options.count("dbstring") ? options["dbstring"].as<std::string>() : std::string();
        my->config->schema        = options["pg-schema"].as<std::string>();
        my->config->skip_to       = options.count("fill-skip-to") ? options["fill-skip-to"].as<uint32_t>() : 0;
        my->config->stop_before   = options.count("fill-stop") ? options["fill-stop"].as<uint32_t>() : 0;
        my->config->trx_filters   = fill_plugin::get_trx_filters(options);
        my->config->drop_schema   = options.count("fpg-drop");
        my->config->create_schema = options.count("fpg-create");
        my->config->enable_trim   = options.count("fill-trim");
        my->config->ignore_on_block = options.count("ignore-onblock");
		my->config->remove_old_delta_row = options.count("remove_old_delta_row");
    }
    FC_LOG_AND_RETHROW()
}

void fill_pg_plugin::plugin_startup() { my->start(); }

void fill_pg_plugin::plugin_shutdown() {
    if (my->session)
        my->session->connection->close(false);
    my->timer.cancel();
    ilog("fill_pg_plugin stopped");
}
