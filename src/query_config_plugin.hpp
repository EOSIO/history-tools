// copyright defined in LICENSE.txt

#pragma once
#include <appbase/application.hpp>

class query_config_plugin : public appbase::plugin<query_config_plugin> {
  public:
    APPBASE_PLUGIN_REQUIRES()

    query_config_plugin();
    virtual ~query_config_plugin();

    virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
    void         plugin_initialize(const appbase::variables_map& options);
    void         plugin_startup();
    void         plugin_shutdown();
};
