#include "wasm_plugin.hpp"
#include "wasm_dispatcher.hpp"

#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>

using namespace appbase;
using namespace std::literals;
using namespace abieos::literals;

static abstract_plugin& _wasm_plugin = app().register_plugin<wasm_plugin>();

struct wasm_plugin_impl : std::enable_shared_from_this<wasm_plugin_impl> {
    bool                           stopping = false;
    std::string                    wasm     = {};
    history_tools::wasm_dispatcher dispatcher;

    void start() { dispatcher.create("eosio.hist"_n, wasm, true, {}, {}, {}, {}); }

    void shutdown() { stopping = true; }
}; // wasm_plugin_impl

wasm_plugin::wasm_plugin()
    : my(std::make_shared<wasm_plugin_impl>()) {}

wasm_plugin::~wasm_plugin() {
    if (my->stopping)
        ilog("wasm_plugin stopped");
}

void wasm_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto op = cfg.add_options();
    op("wasm", bpo::value<std::string>(), "wasm to execute on startup");
}

void wasm_plugin::plugin_initialize(const variables_map& options) {
    history_tools::wasm_dispatcher::register_callbacks();
    try {
        if (!options.count("wasm"))
            throw std::runtime_error("--wasm is required");
        my->wasm = options.at("wasm").as<std::string>();
    }
    FC_LOG_AND_RETHROW()
}

void wasm_plugin::plugin_startup() { my->start(); }
void wasm_plugin::plugin_shutdown() { my->shutdown(); }
