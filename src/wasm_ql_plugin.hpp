// copyright defined in LICENSE.txt

#pragma once
#include <appbase/application.hpp>

#include "query_config.hpp"
#include "state_history.hpp"

struct query_session {
    virtual ~query_session() {}

    virtual state_history::fill_status         get_fill_status()                                     = 0;
    virtual std::optional<abieos::checksum256> get_block_id(uint32_t block_index)                    = 0;
    virtual std::vector<char>                  exec_query(abieos::input_buffer query, uint32_t head) = 0;
};

struct database_interface {
    virtual ~database_interface() {}

    virtual std::unique_ptr<query_session> create_query_session() = 0;
};

class wasm_ql_plugin : public appbase::plugin<wasm_ql_plugin> {
  public:
    APPBASE_PLUGIN_REQUIRES((wasm_ql_plugin))

    wasm_ql_plugin();
    virtual ~wasm_ql_plugin();

    virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
    void         plugin_initialize(const appbase::variables_map& options);
    void         plugin_startup();
    void         plugin_shutdown();

    void set_database(std::shared_ptr<database_interface> db_iface);

  private:
    std::shared_ptr<struct wasm_ql_plugin_impl> my;
};
