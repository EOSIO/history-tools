// copyright defined in LICENSE.txt

#include "lmdb_plugin.hpp"
#include "util.hpp"

#include <fc/exception/exception.hpp>

using namespace appbase;
using namespace std::literals;

struct lmdb_plugin_impl {
    boost::filesystem::path      config_path = {};
    boost::filesystem::path      db_path     = {};
    uint32_t                     db_size     = {};
    std::shared_ptr<::lmdb_inst> lmdb_inst   = {};
};

static abstract_plugin& _lmdb_plugin = app().register_plugin<lmdb_plugin>();

lmdb_plugin::lmdb_plugin()
    : my(std::make_shared<lmdb_plugin_impl>()) {}

lmdb_plugin::~lmdb_plugin() {}

void lmdb_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto op = cfg.add_options();
    op("lmdb-query-config", bpo::value<std::string>()->default_value("../src/query-config.json"), "Query configuration");
    op("lmdb-database", bpo::value<std::string>()->default_value("./chain.lmdb"), "Database path");
    op("lmdb-set-db-size-gb", bpo::value<uint32_t>(), "Increase database maximum size to [arg]. This value is written into the database.");
}

void lmdb_plugin::plugin_initialize(const variables_map& options) {
    try {
        my->config_path = options["lmdb-query-config"].as<std::string>().c_str();
        my->db_path     = options["lmdb-database"].as<std::string>();
        my->db_size     = options.count("lmdb-set-db-size-gb") ? options["lmdb-set-db-size-gb"].as<uint32_t>() : 0;
    }
    FC_LOG_AND_RETHROW()
}

void lmdb_plugin::plugin_startup() {
    ilog("using database ${db}", ("db", my->db_path.c_str()));
    my->lmdb_inst = std::make_shared<lmdb_inst>(my->db_path, my->db_size);
    try {
        ilog("using query config ${qc}", ("qc", my->config_path.c_str()));
        abieos::json_to_native(my->lmdb_inst->query_config, read_string(my->config_path.c_str()));
        my->lmdb_inst->query_config.prepare(state_history::lmdb::abi_type_to_lmdb_type);
    } catch (const std::exception& e) {
        throw std::runtime_error("error processing "s + my->config_path.c_str() + ": " + e.what());
    }
}

void lmdb_plugin::plugin_shutdown() {}

std::shared_ptr<lmdb_inst> lmdb_plugin::get_lmdb_inst() { return my->lmdb_inst; }
