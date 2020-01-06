// copyright defined in LICENSE.txt

#pragma once
#include "wasm_ql.hpp"

namespace wasm_ql {

struct http_server {
    virtual ~http_server() {}

    static std::shared_ptr<http_server> create( //
        int num_threads, const std::shared_ptr<const shared_state>& state, const std::string& address, const std::string& port);

    virtual void stop() = 0;
};

} // namespace wasm_ql
