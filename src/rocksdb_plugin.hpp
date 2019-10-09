// copyright defined in LICENSE.txt

#pragma once
#include <appbase/application.hpp>

#include "query_config_plugin.hpp"
#include "state_history_rocksdb.hpp"

struct rocksdb_inst {
    state_history::rdb::database database;
    state_history::kv::config    query_config{};

    rocksdb_inst(const char* db_path, const char* ro_path, std::optional<uint32_t> threads, std::optional<uint32_t> max_open_files)
        : database{db_path, ro_path, threads, max_open_files} {}
};

class rocksdb_plugin : public appbase::plugin<rocksdb_plugin> {
  public:
    APPBASE_PLUGIN_REQUIRES((query_config_plugin))

    rocksdb_plugin();
    virtual ~rocksdb_plugin();

    virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
    void         plugin_initialize(const appbase::variables_map& options);
    void         plugin_startup();
    void         plugin_shutdown();

    std::shared_ptr<rocksdb_inst> get_rocksdb_inst_rw();
    std::shared_ptr<rocksdb_inst> get_rocksdb_inst_ro();

  private:
    std::shared_ptr<struct rocksdb_plugin_impl> my;
};
