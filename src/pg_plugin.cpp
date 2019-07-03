// copyright defined in LICENSE.txt

#include "pg_plugin.hpp"
#include "util.hpp"

#include <fc/exception/exception.hpp>

using namespace appbase;
using namespace std::literals;

static abstract_plugin& _pg_plugin = app().register_plugin<pg_plugin>();

pg_plugin::pg_plugin() {}
pg_plugin::~pg_plugin() {}

void pg_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto op = cfg.add_options();
    op("pg-schema", bpo::value<std::string>()->default_value("chain"), "Database schema");
}

void pg_plugin::plugin_initialize(const variables_map& options) {}
void pg_plugin::plugin_startup() {}
void pg_plugin::plugin_shutdown() {}
