// copyright defined in LICENSE.txt

#pragma once

#include "wasm_ql_plugin.hpp"

using wasm_ql_pg_ptr = std::shared_ptr<struct wasm_ql_pg_plugin_impl>;

class wasm_ql_pg_plugin : public appbase::plugin<wasm_ql_pg_plugin> {
  public:
    APPBASE_PLUGIN_REQUIRES()

    wasm_ql_pg_plugin();
    virtual ~wasm_ql_pg_plugin();

    virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;

    void plugin_initialize(const appbase::variables_map& options);
    void plugin_startup();
    void plugin_shutdown();

  private:
    wasm_ql_pg_ptr my;
};
