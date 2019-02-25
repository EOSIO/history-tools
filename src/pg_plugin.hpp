// copyright defined in LICENSE.txt

#pragma once
#include <appbase/application.hpp>

class pg_plugin : public appbase::plugin<pg_plugin> {
  public:
    APPBASE_PLUGIN_REQUIRES()

    pg_plugin();
    virtual ~pg_plugin();

    virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
    void         plugin_initialize(const appbase::variables_map& options);
    void         plugin_startup();
    void         plugin_shutdown();
};
