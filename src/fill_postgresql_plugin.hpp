// copyright defined in LICENSE.txt

#pragma once
#include <appbase/application.hpp>

class fill_postgresql_plugin : public appbase::plugin<fill_postgresql_plugin> {
  public:
    APPBASE_PLUGIN_REQUIRES()

    fill_postgresql_plugin();
    virtual ~fill_postgresql_plugin();

    virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
    void         plugin_initialize(const appbase::variables_map& options);
    void         plugin_startup();
    void         plugin_shutdown();

  private:
    std::shared_ptr<struct fill_postgresql_plugin_impl> my;
};
