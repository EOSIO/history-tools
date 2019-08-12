// copyright defined in LICENSE.txt

#include "rocksdb_plugin.hpp"
#include "util.hpp"

#include <fc/exception/exception.hpp>

using namespace appbase;
using namespace std::literals;

struct rocksdb_plugin_impl {
    boost::filesystem::path         config_path     = {};
    boost::filesystem::path         db_path         = {};
    boost::filesystem::path         db_2nd_path     = {};
    std::optional<uint32_t>         threads         = {};
    std::optional<uint32_t>         max_open_files  = {};
    std::shared_ptr<::rocksdb_inst> rocksdb_inst_rw = {};
    std::shared_ptr<::rocksdb_inst> rocksdb_inst_ro = {};
};

static abstract_plugin& _rocksdb_plugin = app().register_plugin<rocksdb_plugin>();

rocksdb_plugin::rocksdb_plugin()
    : my(std::make_shared<rocksdb_plugin_impl>()) {}

rocksdb_plugin::~rocksdb_plugin() {}

void rocksdb_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto op = cfg.add_options();
    op("rdb-database", bpo::value<std::string>()->default_value("./chain.rocksdb"), "Primary database path");
    op("rdb-secondary", bpo::value<std::string>()->default_value("./chain.rocksdb.2nd"),
       "Path for secondary database instance. Only used by wasm_ql_pg_plugin. This database automatically tails the primary "
       "database.");
    op("rdb-threads", bpo::value<uint32_t>(),
       "Increase number of background RocksDB threads. Only used with fill_rocksdb_plugin. Recommend 8 for full history on "
       "large chains.");
    op("rdb-max-files", bpo::value<uint32_t>(),
       "RocksDB limit max number of open files (default unlimited). This is a last-resort option; 'ulimit -n #', where # is a very large "
       "number is usually a better option. Only used with fill_rocksdb_plugin; wasm_ql_pg_plugin is always unlimited.");
}

void rocksdb_plugin::plugin_initialize(const variables_map& options) {
    try {
        my->config_path = options["query-config"].as<std::string>().c_str();
        my->db_path     = options["rdb-database"].as<std::string>();
        my->db_2nd_path = options["rdb-secondary"].as<std::string>();
        if (!options["rdb-threads"].empty())
            my->threads = options["rdb-threads"].as<uint32_t>();
        if (!options["rdb-max-files"].empty())
            my->max_open_files = options["rdb-max-files"].as<uint32_t>();
    }
    FC_LOG_AND_RETHROW()
}

void rocksdb_plugin::plugin_startup() {}

void rocksdb_plugin::plugin_shutdown() {}

static void open_query_config(rocksdb_plugin_impl* my, std::shared_ptr<rocksdb_inst>& inst) {
    try {
        ilog("using query config ${qc}", ("qc", my->config_path.c_str()));
        abieos::json_to_native(inst->query_config, read_string(my->config_path.c_str()));
        inst->query_config.prepare(state_history::kv::abi_type_to_kv_type);
    } catch (const std::exception& e) {
        inst.reset();
        throw std::runtime_error("error processing "s + my->config_path.c_str() + ": " + e.what());
    }
}

std::shared_ptr<rocksdb_inst> rocksdb_plugin::get_rocksdb_inst_rw() {
    if (!my->rocksdb_inst_rw) {
        my->rocksdb_inst_rw = std::make_shared<rocksdb_inst>(my->db_path.c_str(), nullptr, my->threads, my->max_open_files);
        open_query_config(my.get(), my->rocksdb_inst_rw);
    }
    return my->rocksdb_inst_rw;
}

std::shared_ptr<rocksdb_inst> rocksdb_plugin::get_rocksdb_inst_ro() {
    if (!my->rocksdb_inst_ro) {
        my->rocksdb_inst_ro = std::make_shared<rocksdb_inst>(my->db_path.c_str(), my->db_2nd_path.c_str(), std::nullopt, std::nullopt);
        open_query_config(my.get(), my->rocksdb_inst_ro);
    }
    return my->rocksdb_inst_ro;
}
