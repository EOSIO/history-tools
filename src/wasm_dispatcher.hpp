#pragma once
#include "abieos.hpp"

namespace history_tools {

struct wasm_dispatcher_impl;

class wasm_dispatcher {
  public:
    wasm_dispatcher();

    static void register_callbacks();
    void        add_ship_connection(abieos::name name, std::string host, std::string port);

    void create(                                           //
        abieos::name              name,                    //
        std::string               wasm,                    //
        bool                      privileged,              //
        std::vector<std::string>  args,                    //
        std::vector<abieos::name> database_write_contexts, //
        std::vector<abieos::name> ships,                   //
        std::vector<std::string>  api_handlers);

  private:
    std::shared_ptr<wasm_dispatcher_impl> my;
};

} // namespace history_tools
