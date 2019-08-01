// copyright defined in LICENSE.txt

#include "rocksdb_plugin.hpp"
#include "util.hpp"

#include <fc/exception/exception.hpp>

using namespace appbase;
using namespace std::literals;

struct rocksdb_plugin_impl {
    boost::filesystem::path         config_path    = {};
    boost::filesystem::path         db_path        = {};
    std::optional<uint32_t>         threads        = {};
    std::optional<uint32_t>         max_open_files = {};
    std::shared_ptr<::rocksdb_inst> rocksdb_inst   = {};
};

static abstract_plugin& _rocksdb_plugin = app().register_plugin<rocksdb_plugin>();

rocksdb_plugin::rocksdb_plugin()
    : my(std::make_shared<rocksdb_plugin_impl>()) {}

rocksdb_plugin::~rocksdb_plugin() {}

void rocksdb_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto op = cfg.add_options();
    op("rdb-database", bpo::value<std::string>()->default_value("./chain.rocksdb"), "Database path");
    op("rdb-threads", bpo::value<uint32_t>(),
       "Increase number of background RocksDB threads. Only needed for 'fill-rocksdb' and 'combo-rocksdb'. Recommend 8 for full history on "
       "large chains.");
    op("rdb-max-files", bpo::value<uint32_t>(),
       "RocksDB limit max number of open files (default unlimited). Set this if you hit a 'too many open files' error. Recommend 40000 for "
       "full history on large chains. Linux also needs 'ulimit -n #', where # is larger than --rocksdb-open-files.");
}

void rocksdb_plugin::plugin_initialize(const variables_map& options) {
    try {
        my->config_path = options["query-config"].as<std::string>().c_str();
        my->db_path     = options["rdb-database"].as<std::string>();
        if (!options["rdb-threads"].empty())
            my->threads = options["rdb-threads"].as<uint32_t>();
        if (!options["rdb-max-files"].empty())
            my->max_open_files = options["rdb-max-files"].as<uint32_t>();
    }
    FC_LOG_AND_RETHROW()
}

void rocksdb_plugin::plugin_startup() {
    ilog("using database ${db}", ("db", my->db_path.c_str()));
    my->rocksdb_inst = std::make_shared<rocksdb_inst>(my->db_path, my->threads, my->max_open_files);
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
