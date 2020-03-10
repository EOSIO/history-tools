// copyright defined in LICENSE.txt

#pragma once
#include <eosio/history-tools/callbacks/action.hpp>
#include <eosio/history-tools/callbacks/console.hpp>
#include <eosio/history-tools/callbacks/kv.hpp>
#include "wasm_ql_plugin.hpp"

#include <eosio/vm/backend.hpp>

namespace eosio { namespace wasm_ql {

class backend_cache;

struct shared_state {
   uint32_t                            max_console_size = {};
   uint32_t                            wasm_cache_size  = {};
   uint64_t                            max_exec_time_ms = {};
   std::string                         contract_dir     = {};
   std::unique_ptr<backend_cache>      backend_cache    = {};
   std::shared_ptr<chain_kv::database> db;

   shared_state(std::shared_ptr<chain_kv::database> db);
   ~shared_state();
};

struct thread_state : eosio::history_tools::action_state, eosio::history_tools::console_state {
   std::shared_ptr<const shared_state> shared = {};
   eosio::vm::wasm_allocator           wa     = {};
};

void register_callbacks();

const std::vector<char>& query_get_info(wasm_ql::thread_state& thread_state);
const std::vector<char>& query_get_block(wasm_ql::thread_state& thread_state, std::string_view body);
const std::vector<char>& query_get_abi(wasm_ql::thread_state& thread_state, std::string_view body);
const std::vector<char>& query_get_required_keys(wasm_ql::thread_state& thread_state, std::string_view body);
const std::vector<char>& query_send_transaction(wasm_ql::thread_state& thread_state, std::string_view body);

}} // namespace eosio::wasm_ql
