#pragma once
#include <appbase/application.hpp>

class wasm_plugin : public appbase::plugin<wasm_plugin> {
  public:
    APPBASE_PLUGIN_REQUIRES()

    wasm_plugin();
    virtual ~wasm_plugin();

    void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
    void plugin_initialize(const appbase::variables_map& options);
    void plugin_startup();
    void plugin_shutdown();

  private:
    std::shared_ptr<struct wasm_plugin_impl> my;
};
