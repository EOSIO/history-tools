// copyright defined in LICENSE.txt

#include "lmdb_plugin.hpp"
#include "util.hpp"

#include <fc/exception/exception.hpp>

using namespace appbase;

struct lmdb_plugin_impl {
    std::shared_ptr<::lmdb_inst> lmdb_inst;
};

static abstract_plugin& _lmdb_plugin = app().register_plugin<lmdb_plugin>();

lmdb_plugin::lmdb_plugin()
    : my(std::make_shared<lmdb_plugin_impl>()) {}

lmdb_plugin::~lmdb_plugin() {}

void lmdb_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto op = cfg.add_options();
    op("lmdb-query-config,q", bpo::value<std::string>()->default_value("../src/query-config.json"), "Query configuration");
    op("lmdb-set-db-size-gb", bpo::value<uint32_t>(),
       "Increase database size to [arg]. This option will grow the database size limit, but not shrink it");
}

void lmdb_plugin::plugin_initialize(const variables_map& options) {
    try {
        auto size     = options.count("lmdb-set-db-size-gb") ? options["lmdb-set-db-size-gb"].as<uint32_t>() : 0;
        my->lmdb_inst = std::make_shared<lmdb_inst>(size);
        auto x        = read_string(options["lmdb-query-config"].as<std::string>().c_str());
        try {
            abieos::json_to_native(my->lmdb_inst->query_config, x);
            my->lmdb_inst->query_config.prepare(state_history::lmdb::abi_type_to_lmdb_type);
        } catch (const std::exception& e) {
            throw std::runtime_error("error processing " + options["lmdb-query-config"].as<std::string>() + ": " + e.what());
        }
    }
    FC_LOG_AND_RETHROW()
}

void lmdb_plugin::plugin_startup() {}

void lmdb_plugin::plugin_shutdown() {}

std::shared_ptr<lmdb_inst> lmdb_plugin::get_lmdb_inst() { return my->lmdb_inst; }
