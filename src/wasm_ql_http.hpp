// copyright defined in LICENSE.txt

#pragma once
#include "wasm_ql.hpp"

namespace wasm_ql {

struct http_config {
   uint32_t    num_threads      = {};
   uint32_t    max_request_size = {};
   uint32_t    idle_timeout     = {};
   std::string allow_origin     = {};
   std::string static_dir       = {};
   std::string address          = {};
   std::string port             = {};
};

struct http_server {
   virtual ~http_server() {}

   static std::shared_ptr<http_server> create(const std::shared_ptr<const http_config>&  http_config,
                                              const std::shared_ptr<const shared_state>& shared_state);

   virtual void stop() = 0;
};

} // namespace wasm_ql
