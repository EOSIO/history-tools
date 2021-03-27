// copyright defined in LICENSE.txt

#include "fill_pg_plugin.hpp"
#include "state_history_connection.hpp"
#include "state_history_pg.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "abieos_sql_converter.hpp"
#include <pqxx/tablewriter>

using namespace abieos;
using namespace appbase;
using namespace eosio::ship_protocol;
using namespace state_history;
using namespace state_history::pg;
using namespace std::literals;

namespace asio = boost::asio;
namespace bpo  = boost::program_options;

using asio::ip::tcp;
using boost::beast::flat_buffer;
using boost::system::error_code;

inline std::string to_string(const eosio::checksum256& v) { return sql_str(v); }

inline std::string quote(std::string s) { return "'" + s + "'"; }

/// a wrapper class for pqxx::work to log the SQL command sent to database
struct work_t {
    pqxx::work w;

    work_t(pqxx::connection& conn)
        : w(conn) {}

    auto exec(std::string stmt) {
        dlog(stmt.c_str());
        return w.exec(stmt);
    }

    void commit() { w.commit(); }

    auto quote_name(const std::string& str) { return w.quote_name(str); }
};

/// a wrapper class for pqxx::pipeline to log the SQL command sent to database
struct pipeline_t {
    pqxx::pipeline p;

    pipeline_t(work_t& w)
        : p(w.w) {}

    auto insert(std::string stmt) -> decltype(p.insert(stmt)) {
        dlog(stmt.c_str());
        return p.insert(stmt);
    }

    template <typename T>
    auto retrieve(T&& t) {
        return p.retrieve(std::move(t));
    }

    auto retrieve() { return p.retrieve(); }

    bool empty() const { return p.empty(); }

    void complete() { p.complete(); }
};

/// a wrapper class for pqxx::tablewriter to log the write_raw_line()
struct tablewriter {
    pqxx::tablewriter wr;
    tablewriter(work_t& t, const std::string& name)
        : wr(t.w, name) {}

    void write_raw_line(std::string v) { wr.write_raw_line(v); }
    void complete() { wr.complete(); }
};

struct table_stream {
    pqxx::connection c;
    work_t           t;
    tablewriter      writer;

    table_stream(const std::string& name)
        : t(c)
        , writer(t, name) {}
};

struct fpg_session;

struct fill_postgresql_config : connection_config {
    std::string             schema;
    uint32_t                skip_to       = 0;
    uint32_t                stop_before   = 0;
    std::vector<trx_filter> trx_filters   = {};
    bool                    drop_schema   = false;
    bool                    create_schema = false;
    bool                    enable_trim   = false;
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

abi_type& get_type(std::map<std::string, abi_type>& abi_types, std::string type_name) {
    auto itr = abi_types.find(type_name);
    if (itr != abi_types.end()) {
        return itr->second;
    }
    throw std::runtime_error("Unable to find " + type_name + " in the received abi");
}

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
    abieos_sql_converter                                 converter;
    std::map<std::string, abi_type>                      abi_types;

    fpg_session(fill_postgresql_plugin_impl* my)
        : my(my)
        , config(my->config) {

        ilog("connect to postgresql");
        sql_connection.emplace();
        converter.schema_name = sql_connection->quote_name(config->schema);
    }

    std::string quote_name(std::string name) { return sql_connection->quote_name(name); }

    void start(asio::io_context& ioc) {
        if (config->drop_schema) {
            work_t t(*sql_connection);
            t.exec("drop schema if exists " + converter.schema_name + " cascade");
            t.commit();
            config->drop_schema = false;
        }

        connection = std::make_shared<state_history::connection>(ioc, *config, shared_from_this());
        connection->connect();
    }

    abi_type& get_type(std::string type_name) { return ::get_type(this->abi_types, type_name); }

    void received_abi(eosio::abi&& abi) override {
        auto& transaction_trace_abi = ::get_type(abi.abi_types, "transaction_trace");
        for (auto& member : std::get<eosio::abi_type::variant>(transaction_trace_abi._data)) {
            auto& member_abi = ::get_type(abi.abi_types, member.name);
            for (auto& field : std::get<eosio::abi_type::struct_>(member_abi._data).fields) {
                if (field.name == "status")
                    field.type = &abi.abi_types.try_emplace("transaction_status", "transaction_status", eosio::abi_type::builtin{}, nullptr)
                                      .first->second;
                else if (field.name == "failed_dtrx_trace" && field.type->name == "transaction_trace?") {
                    field.type = eosio::add_type(abi, (std::vector<eosio::ship_protocol::recurse_transaction_trace>*)nullptr);
                }
            }
        }

        abi_types = std::move(abi.abi_types);

        if (config->create_schema) {
            create_tables();
            config->create_schema = false;
        }
        connection->send(get_status_request_v0{});
    }

    bool received(get_status_result_v0& status) override {
        work_t t(*sql_connection);
        load_fill_status(t);
        auto       positions = get_positions(t);
        pipeline_t pipeline(t);
        truncate(t, pipeline, head + 1);
        pipeline.complete();
        t.commit();

        connection->request_blocks(status, std::max(config->skip_to, head + 1), positions);
        return true;
    }

    void create_tables() {
        work_t t(*sql_connection);

        ilog("create schema ${s}", ("s", converter.schema_name));
        t.exec("create schema " + converter.schema_name);
        t.exec(
            "create type " + converter.schema_name +
            ".transaction_status_type as enum('executed', 'soft_fail', 'hard_fail', 'delayed', 'expired')");
        t.exec(
            "create table " + converter.schema_name +
            R"(.received_block ("block_num" bigint, "block_id" varchar(64), primary key("block_num")))");
        t.exec(
            "create table " + converter.schema_name +
            R"(.fill_status ("head" bigint, "head_id" varchar(64), "irreversible" bigint, "irreversible_id" varchar(64), "first" bigint))");
        t.exec("create unique index on " + converter.schema_name + R"(.fill_status ((true)))");
        t.exec("insert into " + converter.schema_name + R"(.fill_status values (0, '', 0, '', 0))");

        auto exec = [&t](const auto& stmt) { t.exec(stmt); };
        converter.create_table("block_info", get_type("block_header"), "block_num bigint, block_id varchar(64)", {"block_num"}, exec);

        converter.create_table(
            "transaction_trace", get_type("transaction_trace"), "block_num bigint, transaction_ordinal integer",
            {"block_num", "transaction_ordinal"}, exec);

        for (auto& table : connection->abi.tables) {
            std::vector<std::string> keys = {"block_num", "present"};
            keys.insert(keys.end(), table.key_names.begin(), table.key_names.end());
            converter.create_table(table.type, get_type(table.type), "block_num bigint, present smallint", keys, exec);
        }

        t.commit();
    } // create_tables()

    void create_trim() {
        if (created_trim)
            return;
        work_t t(*sql_connection);
        ilog("create_trim");
        for (auto& table : connection->abi.tables) {
            if (table.key_names.empty())
                continue;
            std::string query = "create index if not exists " + table.type;
            for (auto& k : table.key_names)
                query += "_" + k;
            query += "_block_present_idx on " + converter.schema_name + "." + quote_name(table.type) + "(\n";
            for (auto& k : table.key_names)
                query += "    " + quote_name(k) + ",\n";
            query += "    \"block_num\" desc,\n    \"present\" desc\n)";
            // std::cout << query << ";\n\n";
            t.exec(query);
        }

        std::string query = R"(
            drop function if exists )" +
                            converter.schema_name + R"(.trim_history;
        )";
        // std::cout << query << "\n";
        t.exec(query);

        query = R"(
            create function )" +
                converter.schema_name + R"(.trim_history(
                prev_block_num bigint,
                irrev_block_num bigint
            ) returns void
            as $$
                declare
                    key_search record;
                begin)";

        static const char* const simple_cases[] = {
            "received_block",
            "transaction_trace",
            "block_info",
        };

        for (const char* table : simple_cases) {
            query += R"(
                    delete from )" +
                     converter.schema_name + "." + quote_name(table) + R"(
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
                     converter.schema_name + "." + quote_name(table) + R"(
                        where
                            block_num > prev_block_num and block_num <= irrev_block_num
                        order by )" +
                     keys + R"(, block_num desc, present desc
                    loop
                        delete from )" +
                     converter.schema_name + "." + quote_name(table) + R"(
                        where
                            ()" +
                     keys + R"() = ()" + search_keys + R"()
                            and block_num < key_search.block_num;
                    end loop;
                    )";
        };

        for (auto& table : connection->abi.tables) {
            if (table.key_names.empty()) {
                query += R"(
                    for key_search in
                        select
                            block_num
                        from
                            )" +
                         converter.schema_name + "." + quote_name(table.type) + R"(
                        where
                            block_num > prev_block_num and block_num <= irrev_block_num
                        order by block_num desc, present desc
                        limit 1
                    loop
                        delete from )" +
                         converter.schema_name + "." + quote_name(table.type) + R"(
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
                    keys += quote_name(k);
                    search_keys += "key_search." + quote_name(k);
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

    void load_fill_status(work_t& t) {
        auto r  = t.exec("select head, head_id, irreversible, irreversible_id, first from " + converter.schema_name + ".fill_status")[0];
        head    = r[0].as<uint32_t>();
        head_id = r[1].as<std::string>();
        irreversible    = r[2].as<uint32_t>();
        irreversible_id = r[3].as<std::string>();
        first           = r[4].as<uint32_t>();
    }

    std::vector<block_position> get_positions(work_t& t) {
        std::vector<block_position> result;
        auto                        rows = t.exec(
            "select block_num, block_id from " + converter.schema_name + ".received_block where block_num >= " +
            std::to_string(irreversible) + " and block_num <= " + std::to_string(head) + " order by block_num");
        for (auto row : rows)
            result.push_back({row[0].as<uint32_t>(), sql_to_checksum256(row[1].as<std::string>().c_str())});
        return result;
    }

    void write_fill_status(work_t& t, pipeline_t& pipeline) {
        std::string query =
            "update " + converter.schema_name + ".fill_status set head=" + std::to_string(head) + ", head_id=" + quote(head_id) + ", ";
        if (irreversible < head)
            query += "irreversible=" + std::to_string(irreversible) + ", irreversible_id=" + quote(irreversible_id);
        else
            query += "irreversible=" + std::to_string(head) + ", irreversible_id=" + quote(head_id);
        query += ", first=" + std::to_string(first);
        pipeline.insert(query);
    }

    void truncate(work_t& t, pipeline_t& pipeline, uint32_t block) {
        auto trunc = [&](const std::string& name) {
            std::string query{"delete from " + converter.schema_name + "." + quote_name(name) +
                              " where block_num >= " + std::to_string(block)};
            pipeline.insert(query);
        };
        trunc("received_block");
        trunc("transaction_trace");
        trunc("block_info");
        for (auto& table : connection->abi.tables) {
            trunc(table.type);
        }

        auto result = pipeline.retrieve(pipeline.insert(
            "select block_id from " + converter.schema_name + ".received_block where block_num=" + std::to_string(block - 1)));
        if (result.empty()) {
            head    = 0;
            head_id = "";
        } else {
            head    = block - 1;
            head_id = result.front()[0].as<std::string>();
        }
        first = std::min(first, head);
    } // truncate

    bool received(get_blocks_result_v1& result) override {
        if (!result.this_block)
            return true;
        bool bulk         = result.this_block->block_num + 4 < result.last_irreversible.block_num;
        bool large_deltas = false;
        auto deltas_size  = result.deltas.num_bytes();
        if (!bulk && deltas_size >= 10 * 1024 * 1024) {
            ilog("large deltas size: ${s}", ("s", uint64_t(deltas_size)));
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

        work_t     t(*sql_connection);
        pipeline_t pipeline(t);
        if (result.this_block->block_num <= head)
            truncate(t, pipeline, result.this_block->block_num);
        if (!head_id.empty() && (!result.prev_block || to_string(result.prev_block->block_id) != head_id))
            throw std::runtime_error("prev_block does not match");
        if (result.block)
            receive_block(result.this_block->block_num, result.this_block->block_id, result.block.value());
        if (deltas_size)
            receive_deltas(result.this_block->block_num, result.deltas, bulk);
        if (!result.traces.empty())
            receive_traces(result.this_block->block_num, result.traces);

        head            = result.this_block->block_num;
        head_id         = to_string(result.this_block->block_id);
        irreversible    = result.last_irreversible.block_num;
        irreversible_id = to_string(result.last_irreversible.block_id);
        if (!first)
            first = head;
        if (!bulk) {
            flush_streams();
            write_fill_status(t, pipeline);
        }
        pipeline.insert(
            "insert into " + converter.schema_name + ".received_block (block_num, block_id) values (" +
            std::to_string(result.this_block->block_num) + ", " + quote(to_string(result.this_block->block_id)) + ")");

        pipeline.complete();
        while (!pipeline.empty())
            pipeline.retrieve();
        t.commit();
        if (large_deltas)
            close_streams();
        return true;
    }

    bool received(get_blocks_result_v0& result) override {
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

        work_t     t(*sql_connection);
        pipeline_t pipeline(t);
        if (result.this_block->block_num <= head)
            truncate(t, pipeline, result.this_block->block_num);
        if (!head_id.empty() && (!result.prev_block || to_string(result.prev_block->block_id) != head_id))
            throw std::runtime_error("prev_block does not match");
        if (result.block) {
            auto     block_bin = *result.block;
            uint32_t variant_index;
            varuint32_from_bin(variant_index, block_bin);
            receive_block(result.this_block->block_num, result.this_block->block_id, eosio::as_opaque<block_header>(block_bin));
        }
        if (result.deltas)
            receive_deltas(
                result.this_block->block_num, eosio::as_opaque<std::vector<eosio::ship_protocol::table_delta>>(*result.deltas), bulk);
        if (result.traces)
            receive_traces(
                result.this_block->block_num, eosio::as_opaque<std::vector<eosio::ship_protocol::transaction_trace>>(*result.traces));

        head            = result.this_block->block_num;
        head_id         = to_string(result.this_block->block_id);
        irreversible    = result.last_irreversible.block_num;
        irreversible_id = to_string(result.last_irreversible.block_id);
        if (!first)
            first = head;
        if (!bulk) {
            flush_streams();
            write_fill_status(t, pipeline);
        }
        pipeline.insert(
            "insert into " + converter.schema_name + ".received_block (block_num, block_id) values (" +
            std::to_string(result.this_block->block_num) + ", " + quote(to_string(result.this_block->block_id)) + ")");

        pipeline.complete();
        while (!pipeline.empty())
            pipeline.retrieve();
        t.commit();
        if (large_deltas)
            close_streams();
        return true;
    } // receive_result()

    void write_stream(uint32_t block_num, const std::string& name, const std::vector<std::string>& values) {
        if (!first_bulk)
            first_bulk = block_num;
        auto& ts = table_streams[name];
        if (!ts)
            ts = std::make_unique<table_stream>(converter.schema_name + "." + quote_name(name));
        ts->writer.write_raw_line(boost::algorithm::join(values, "\t"));
    }

    void flush_streams() {
        for (auto& [_, ts] : table_streams) {
            ts->writer.complete();
            ts->t.commit();
        }
        table_streams.clear();
    }

    void close_streams() {
        if (table_streams.empty())
            return;
        flush_streams();

        work_t     t(*sql_connection);
        pipeline_t pipeline(t);
        write_fill_status(t, pipeline);
        pipeline.complete();
        t.commit();

        ilog("block ${b} - ${e}", ("b", first_bulk)("e", head));
        first_bulk = 0;
    }

    void receive_block(uint32_t block_num, const checksum256& block_id, signed_block_variant& block) {
        const block_header& header = std::visit([](const auto& v) -> const block_header& { return v; }, block);
        std::vector<char>   data   = eosio::convert_to_bin(header);
        receive_block(block_num, block_id, eosio::as_opaque<block_header>(eosio::input_stream{data}));
    } // receive_block

    void receive_block(uint32_t block_num, const checksum256& block_id, eosio::opaque<block_header> opq) {
        auto&                    abi_type = get_type("block_header");
        std::vector<std::string> values{std::to_string(block_num), sql_str(block_id)};
        auto                     bin = opq.get();
        converter.to_sql_values(bin, *abi_type.as_struct(), values);
        write_stream(block_num, "block_info", values);
    }

    void receive_deltas(uint32_t block_num, eosio::opaque<std::vector<eosio::ship_protocol::table_delta>> delta, bool bulk) {
        for_each(delta, [ this, block_num, bulk ](table_delta&& t_delta){
            write_table_delta(block_num, std::move(t_delta), bulk);
        });
    }

    void write_table_delta(uint32_t block_num, table_delta&& t_delta, bool bulk) {
        std::visit(
            [&block_num, bulk, this](auto t_delta) {
                size_t num_processed = 0;
                auto&  type          = get_type(t_delta.name);
                if (type.as_variant() == nullptr && type.as_struct() == nullptr)
                    throw std::runtime_error("don't know how to process " + t_delta.name);

                for (auto& row : t_delta.rows) {
                    if (t_delta.rows.size() > 10000 && !(num_processed % 10000))
                        ilog(
                            "block ${b} ${t} ${n} of ${r} bulk=${bulk}",
                            ("b", block_num)("t", t_delta.name)("n", num_processed)("r", t_delta.rows.size())("bulk", bulk));

                    std::vector<std::string> values{std::to_string(block_num), std::to_string((unsigned)row.present)};
                    if (type.as_variant())
                        converter.to_sql_values(row.data, t_delta.name, *type.as_variant(), values);
                    else if (type.as_struct())
                        converter.to_sql_values(row.data, *type.as_struct(), values);
                    write_stream(block_num, t_delta.name, values);
                    ++num_processed;
                }
            },
            t_delta);
    }

    void receive_traces(uint32_t block_num, eosio::opaque<std::vector<eosio::ship_protocol::transaction_trace>> traces) {
        auto     bin = traces.get();
        uint32_t num;
        varuint32_from_bin(num, bin);
        uint32_t num_ordinals = 0;
        for (uint32_t i = 0; i < num; ++i) {
            auto              trace_bin = bin;
            transaction_trace trace;
            from_bin(trace, bin);
            if (filter(config->trx_filters, trace))
                write_transaction_trace(block_num, num_ordinals, trace, trace_bin);
        }
    }

    void write_transaction_trace(
        uint32_t block_num, uint32_t& num_ordinals, const eosio::ship_protocol::transaction_trace& trace, eosio::input_stream trace_bin) {

        auto failed = std::visit(
            [](auto& ttrace) { return !ttrace.failed_dtrx_trace.empty() ? &ttrace.failed_dtrx_trace[0].recurse : nullptr; }, trace);
        if (failed != nullptr) {
            if (!filter(config->trx_filters, *failed))
                return;
            std::vector<char> data = eosio::convert_to_bin(*failed);
            write_transaction_trace(block_num, num_ordinals, *failed, eosio::input_stream{data});
        }

        auto                     transaction_ordinal = ++num_ordinals;
        std::vector<std::string> values{std::to_string(block_num), std::to_string(transaction_ordinal)};
        converter.to_sql_values(trace_bin, "transaction_trace", *get_type("transaction_trace").as_variant(), values);
        write_stream(block_num, "transaction_trace", values);
    } // write_transaction_trace

    void trim() {
        if (!config->enable_trim)
            return;
        auto end_trim = std::min(head, irreversible);
        if (first >= end_trim)
            return;
        create_trim();
        work_t t(*sql_connection);
        ilog("trim  ${b} - ${e}", ("b", first)("e", end_trim));
        t.exec("select * from " + converter.schema_name + ".trim_history(" + std::to_string(first) + ", " + std::to_string(end_trim) + ")");
        t.commit();
        ilog("      done");
        first = end_trim;
    }

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
        my->config->schema        = options["pg-schema"].as<std::string>();
        my->config->skip_to       = options.count("fill-skip-to") ? options["fill-skip-to"].as<uint32_t>() : 0;
        my->config->stop_before   = options.count("fill-stop") ? options["fill-stop"].as<uint32_t>() : 0;
        my->config->trx_filters   = fill_plugin::get_trx_filters(options);
        my->config->drop_schema   = options.count("fpg-drop");
        my->config->create_schema = options.count("fpg-create");
        my->config->enable_trim   = options.count("fill-trim");
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
