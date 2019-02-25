// copyright defined in LICENSE.txt

#include "query_config_plugin.hpp"
#include "util.hpp"

#include <fc/exception/exception.hpp>

using namespace appbase;
using namespace std::literals;

static abstract_plugin& _query_config_plugin = app().register_plugin<query_config_plugin>();

query_config_plugin::query_config_plugin() {}
query_config_plugin::~query_config_plugin() {}

void query_config_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto op = cfg.add_options();
    op("query-config", bpo::value<std::string>()->default_value("../src/query-config.json"), "Query configuration");
}

void query_config_plugin::plugin_initialize(const variables_map& options) {}
void query_config_plugin::plugin_startup() {}
void query_config_plugin::plugin_shutdown() {}
