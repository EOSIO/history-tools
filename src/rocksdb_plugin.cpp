// copyright defined in LICENSE.txt

#include "rocksdb_plugin.hpp"
#include "util.hpp"

#include <fc/exception/exception.hpp>

using namespace appbase;
using namespace std::literals;

struct rocksdb_plugin_impl {
    boost::filesystem::path         config_path  = {};
    boost::filesystem::path         db_path      = {};
    std::shared_ptr<::rocksdb_inst> rocksdb_inst = {};
};

static abstract_plugin& _rocksdb_plugin = app().register_plugin<rocksdb_plugin>();

rocksdb_plugin::rocksdb_plugin()
    : my(std::make_shared<rocksdb_plugin_impl>()) {}

rocksdb_plugin::~rocksdb_plugin() {}

void rocksdb_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto op = cfg.add_options();
    op("rocksdb-database", bpo::value<std::string>()->default_value("./chain.rocksdb"), "Database path");
}

void rocksdb_plugin::plugin_initialize(const variables_map& options) {
    try {
        my->config_path = options["query-config"].as<std::string>().c_str();
        my->db_path     = options["rocksdb-database"].as<std::string>();
    }
    FC_LOG_AND_RETHROW()
}

void rocksdb_plugin::plugin_startup() {
    ilog("using database ${db}", ("db", my->db_path.c_str()));
    my->rocksdb_inst = std::make_shared<rocksdb_inst>(my->db_path);
    try {
        ilog("using query config ${qc}", ("qc", my->config_path.c_str()));
        abieos::json_to_native(my->rocksdb_inst->query_config, read_string(my->config_path.c_str()));
        my->rocksdb_inst->query_config.prepare(state_history::kv::abi_type_to_kv_type);
    } catch (const std::exception& e) {
        throw std::runtime_error("error processing "s + my->config_path.c_str() + ": " + e.what());
    }
}

void rocksdb_plugin::plugin_shutdown() {}

std::shared_ptr<rocksdb_inst> rocksdb_plugin::get_rocksdb_inst() { return my->rocksdb_inst; }
