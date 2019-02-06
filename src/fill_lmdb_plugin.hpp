// copyright defined in LICENSE.txt

#pragma once
#include "lmdb_plugin.hpp"

class fill_lmdb_plugin : public appbase::plugin<fill_lmdb_plugin> {
  public:
    APPBASE_PLUGIN_REQUIRES((lmdb_plugin))

    fill_lmdb_plugin();
    virtual ~fill_lmdb_plugin();

    virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
    void         plugin_initialize(const appbase::variables_map& options);
    void         plugin_startup();
    void         plugin_shutdown();

  private:
    std::shared_ptr<struct fill_lmdb_plugin_impl> my;
};
