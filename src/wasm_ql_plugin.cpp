// copyright defined in LICENSE.txt

// todo: timeout wasm
// todo: timeout sql
// todo: what should memory size limit be?
// todo: check callbacks for recursion to limit stack size
// todo: reformulate get_input_data and set_output_data for reentrancy
// todo: wasms get whether a query is present
// todo: indexes on authorized, ram usage, notify
// todo: namespaces for queries
//          A standard namespace
//          ? one for the tokens
// todo: version on queries
//       vector<extendable<...>>
// todo: version on query api?
// todo: better naming for queries

#include "wasm_ql_plugin.hpp"
#include "wasm_ql.hpp"
#include "wasm_ql_http.hpp"

#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>

using namespace appbase;
using namespace wasm_ql;
using namespace std::literals;

static abstract_plugin& _wasm_ql_plugin = app().register_plugin<wasm_ql_plugin>();

struct wasm_ql_plugin_impl : std::enable_shared_from_this<wasm_ql_plugin_impl> {
    bool                                   stopping         = false;
    int                                    num_threads      = {};
    std::string                            endpoint_address = {};
    std::string                            endpoint_port    = {};
    std::shared_ptr<wasm_ql::shared_state> state            = {};
    std::shared_ptr<wasm_ql::http_server>  http_server      = {};

    void start_http() { http_server = wasm_ql::http_server::create(num_threads, state, endpoint_address, endpoint_port); }

    void shutdown() {
        stopping = true;
        if (http_server)
            http_server->stop();
    }
}; // wasm_ql_plugin_impl

wasm_ql_plugin::wasm_ql_plugin()
    : my(std::make_shared<wasm_ql_plugin_impl>()) {}

wasm_ql_plugin::~wasm_ql_plugin() {
    if (my->stopping)
        ilog("wasm_ql_plugin stopped");
}

void wasm_ql_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto op = cfg.add_options();
    op("wql-threads", bpo::value<int>()->default_value(8), "Number of threads to process requests");
    op("wql-listen", bpo::value<std::string>()->default_value("localhost:8880"), "Endpoint to listen on");
    op("wql-allow-origin", bpo::value<std::string>(), "Access-Control-Allow-Origin header. Use \"*\" to allow any.");
    op("wql-wasm-dir", bpo::value<std::string>()->default_value("."), "Directory to fetch WASMs from");
    op("wql-static-dir", bpo::value<std::string>(), "Directory to serve static files from (default: disabled)");
    op("wql-console", "Show console output");
}

void wasm_ql_plugin::plugin_initialize(const variables_map& options) {
    try {
        auto ip_port = options.at("wql-listen").as<std::string>();
        if (ip_port.find(':') == std::string::npos)
            throw std::runtime_error("invalid --wql-listen value: " + ip_port);

        my->state            = std::make_shared<wasm_ql::shared_state>();
        my->state->console   = options.count("wql-console");
        my->num_threads      = options.at("wql-threads").as<int>();
        my->endpoint_port    = ip_port.substr(ip_port.find(':') + 1, ip_port.size());
        my->endpoint_address = ip_port.substr(0, ip_port.find(':'));
        my->state->wasm_dir  = options.at("wql-wasm-dir").as<std::string>();
        if (options.count("wql-allow-origin"))
            my->state->allow_origin = options.at("wql-allow-origin").as<std::string>();
        if (options.count("wql-static-dir"))
            my->state->static_dir = options.at("wql-static-dir").as<std::string>();

        register_callbacks();
    }
    FC_LOG_AND_RETHROW()
}

void wasm_ql_plugin::plugin_startup() {
    if (!my->state->db_iface)
        throw std::runtime_error("wasm_ql_plugin needs either wasm_ql_pg_plugin or wasm_ql_rocksdb_plugin");
    my->start_http();
}
void wasm_ql_plugin::plugin_shutdown() { my->shutdown(); }

void wasm_ql_plugin::set_database(std::shared_ptr<database_interface> db_iface) { my->state->db_iface = std::move(db_iface); }
