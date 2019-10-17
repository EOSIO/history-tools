// copyright defined in LICENSE.txt

#pragma once
#include "wasm_ql_plugin.hpp"

#include <eosio/vm/backend.hpp>

namespace wasm_ql {

struct shared_state {
    bool        console      = {};
    std::string allow_origin = {};
    std::string wasm_dir     = {};
    std::string static_dir   = {};
};

struct thread_state {
    std::shared_ptr<const shared_state> shared  = {};
    eosio::vm::wasm_allocator           wa      = {};
    abieos::input_buffer                request = {}; // todo: rename
    std::vector<char>                   reply   = {}; // todo: rename
};

void                     register_callbacks();
std::vector<char>        query(wasm_ql::thread_state& thread_state, const std::vector<char>& request);
const std::vector<char>& legacy_query(wasm_ql::thread_state& thread_state, const std::string& target, const std::vector<char>& request);

} // namespace wasm_ql
