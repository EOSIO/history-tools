// copyright defined in LICENSE.txt

#include "wasm_ql_pg_plugin.hpp"
#include "state_history_pg.hpp"
#include "util.hpp"

#include <fc/exception/exception.hpp>

using namespace appbase;
namespace pg = state_history::pg;

static abstract_plugin& _wasm_ql_pg_plugin = app().register_plugin<wasm_ql_pg_plugin>();

struct pg_database_interface : database_interface, std::enable_shared_from_this<pg_database_interface> {
    std::string      schema         = {};
    pqxx::connection sql_connection = {};
    pg::config       config         = {};

    virtual ~pg_database_interface() {}

    virtual std::unique_ptr<query_session> create_query_session();
};

struct pg_query_session : query_session {
    virtual ~pg_query_session() {}

    std::shared_ptr<pg_database_interface> db_iface;

    virtual state_history::fill_status get_fill_status() override {
        pqxx::work t(db_iface->sql_connection);
        auto row = t.exec("select head, head_id, irreversible, irreversible_id, first from \"" + db_iface->schema + "\".fill_status")[0];

        state_history::fill_status result;
        result.head            = row[0].as<uint32_t>();
        result.head_id         = pg::sql_to_checksum256(row[1].c_str());
        result.irreversible    = row[2].as<uint32_t>();
        result.irreversible_id = pg::sql_to_checksum256(row[3].c_str());
        result.first           = row[4].as<uint32_t>();
        return result;
    }

    virtual std::optional<abieos::checksum256> get_block_id(uint32_t block_index) override {
        pqxx::work t(db_iface->sql_connection);
        auto       result =
            t.exec("select block_id from \"" + db_iface->schema + "\".block_info where block_index=" + pg::sql_str(false, block_index));
        if (result.empty())
            return {};
        return pg::sql_to_checksum256(result[0][0].c_str());
    }

    virtual std::vector<char> query_database(abieos::input_buffer query_bin, uint32_t head) override {
        abieos::name query_name;
        abieos::bin_to_native(query_name, query_bin);

        auto it = db_iface->config.query_map.find(query_name);
        if (it == db_iface->config.query_map.end())
            throw std::runtime_error("query_database: unknown query: " + (std::string)query_name);
        pg::query& query = *it->second;

        uint32_t max_block_index = 0;
        if (query.limit_block_index)
            max_block_index = std::min(head, abieos::bin_to_native<uint32_t>(query_bin));
        std::string query_str = "select * from \"" + db_iface->schema + "\"." + query.function + "(";
        bool        need_sep  = false;
        if (query.limit_block_index) {
            query_str += pg::sql_str(false, max_block_index);
            need_sep = true;
        }
        auto add_args = [&](auto& args) {
            for (auto& arg : args) {
                if (need_sep)
                    query_str += pg::sep(false);
                query_str += arg.bin_to_sql(db_iface->sql_connection, false, query_bin);
                need_sep = true;
            }
        };
        add_args(query.arg_types);
        add_args(query.range_types);
        add_args(query.range_types);
        auto max_results = abieos::read_raw<uint32_t>(query_bin);
        query_str += pg::sep(false) + pg::sql_str(false, std::min(max_results, query.max_results));
        query_str += ")";

        pqxx::work        t(db_iface->sql_connection);
        auto              exec_result = t.exec(query_str);
        std::vector<char> result;
        std::vector<char> row_bin;
        abieos::push_varuint32(result, exec_result.size());
        for (const auto& r : exec_result) {
            row_bin.clear();
            int i = 0;
            for (auto& type : query.result_types)
                type.sql_to_bin(row_bin, r[i++]);
            if ((uint32_t)row_bin.size() != row_bin.size())
                throw std::runtime_error("query_database: row is too big");
            abieos::push_varuint32(result, row_bin.size());
            result.insert(result.end(), row_bin.begin(), row_bin.end());
        }
        t.commit();
        if ((uint32_t)result.size() != result.size())
            throw std::runtime_error("query_database: result is too big");
        return result;
    }
}; // pg_query_session

std::unique_ptr<query_session> pg_database_interface::create_query_session() {
    auto session      = std::make_unique<pg_query_session>();
    session->db_iface = shared_from_this();
    return session;
}

struct wasm_ql_pg_plugin_impl {
    std::shared_ptr<pg_database_interface> interface;
};

wasm_ql_pg_plugin::wasm_ql_pg_plugin()
    : my(std::make_shared<wasm_ql_pg_plugin_impl>()) {}

wasm_ql_pg_plugin::~wasm_ql_pg_plugin() { ilog("wasm_ql_pg_plugin stopped"); }

void wasm_ql_pg_plugin::set_program_options(options_description& cli, options_description& cfg) {}

void wasm_ql_pg_plugin::plugin_initialize(const variables_map& options) {
    try {
        my->interface         = std::make_shared<pg_database_interface>();
        my->interface->schema = options["pg-schema"].as<std::string>();
        auto x                = read_string(options["query-config"].as<std::string>().c_str());
        try {
            abieos::json_to_native(my->interface->config, x);
        } catch (const std::exception& e) {
            throw std::runtime_error("error processing " + options["query-config"].as<std::string>() + ": " + e.what());
        }
        my->interface->config.prepare(state_history::pg::abi_type_to_sql_type);
        app().find_plugin<wasm_ql_plugin>()->set_database(my->interface);
    }
    FC_LOG_AND_RETHROW()
}

void wasm_ql_pg_plugin::plugin_startup() {}
void wasm_ql_pg_plugin::plugin_shutdown() {}
