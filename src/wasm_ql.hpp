// copyright defined in LICENSE.txt

#pragma once
#include "basic_callbacks.hpp"
#include "state_history_rocksdb.hpp"
#include "wasm_ql_plugin.hpp"

#include <eosio/vm/backend.hpp>

namespace wasm_ql {

struct shared_state {
    bool                                          console      = {};
    std::string                                   allow_origin = {};
    std::string                                   wasm_dir     = {};
    std::string                                   static_dir   = {};
    std::shared_ptr<state_history::rdb::database> db;

    shared_state(std::shared_ptr<state_history::rdb::database> db)
        : db(std::move(db)) {}
};

struct thread_state {
    std::shared_ptr<const shared_state> shared      = {};
    eosio::vm::wasm_allocator           wa          = {};
    abieos::input_buffer                input_data  = {};
    std::vector<char>                   output_data = {};
};

void                     register_callbacks();
const std::vector<char>& query(wasm_ql::thread_state& thread_state, std::string_view wasm, const std::vector<char>& request);
const std::vector<char>& legacy_query(wasm_ql::thread_state& thread_state, const std::string& target, const std::vector<char>& request);

} // namespace wasm_ql
