// copyright defined in LICENSE.txt

#include "fill_plugin.hpp"
#include "util.hpp"

#include <fc/exception/exception.hpp>

using namespace appbase;
using namespace std::literals;

static abstract_plugin& _fill_plugin = app().register_plugin<fill_plugin>();

fill_plugin::fill_plugin() {}
fill_plugin::~fill_plugin() {}

void fill_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto op   = cfg.add_options();
    auto clop = cli.add_options();
    op("fill-connect-to,f", bpo::value<std::string>()->default_value("localhost:8080"), "State-history endpoint to connect to (nodeos)");
    op("fill-trim,t", "Trim history before irreversible");
    clop("fill-skip-to,k", bpo::value<uint32_t>(), "Skip blocks before [arg]");
    clop("fill-stop,x", bpo::value<uint32_t>(), "Stop before block [arg]");
}

void fill_plugin::plugin_initialize(const variables_map& options) {}
void fill_plugin::plugin_startup() {}
void fill_plugin::plugin_shutdown() {}
