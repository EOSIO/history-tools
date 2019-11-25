// copyright defined in LICENSE.txt

#pragma once
#include "state_history_rocksdb.hpp"
#include <appbase/application.hpp>

class rocksdb_plugin : public appbase::plugin<rocksdb_plugin> {
  public:
    APPBASE_PLUGIN_REQUIRES()

    rocksdb_plugin();
    virtual ~rocksdb_plugin();

    virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
    void         plugin_initialize(const appbase::variables_map& options);
    void         plugin_startup();
    void         plugin_shutdown();

    std::shared_ptr<chain_kv::database> get_db();

  private:
    std::shared_ptr<struct rocksdb_plugin_impl> my;
};
