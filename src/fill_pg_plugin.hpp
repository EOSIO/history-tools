// copyright defined in LICENSE.txt

#pragma once
#include "fill_plugin.hpp"
#include <appbase/application.hpp>

class fill_pg_plugin : public appbase::plugin<fill_pg_plugin> {
  public:
    APPBASE_PLUGIN_REQUIRES((fill_plugin))

    fill_pg_plugin();
    virtual ~fill_pg_plugin();

    virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
    void         plugin_initialize(const appbase::variables_map& options);
    void         plugin_startup();
    void         plugin_shutdown();

  private:
    std::shared_ptr<struct fill_postgresql_plugin_impl> my;
};
