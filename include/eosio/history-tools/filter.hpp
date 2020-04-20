#pragma once

#include <eosio/history-tools/callbacks/basic.hpp>
#include <eosio/history-tools/callbacks/chaindb.hpp>
#include <eosio/history-tools/callbacks/compiler_builtins.hpp>
#include <eosio/history-tools/callbacks/console.hpp>
#include <eosio/history-tools/callbacks/filter.hpp>
#include <eosio/history-tools/callbacks/memory.hpp>
#include <eosio/history-tools/callbacks/unimplemented.hpp>
#include <eosio/history-tools/callbacks/unimplemented_filter.hpp>

// todo: configure limits
// todo: timeout
namespace eosio { namespace history_tools { namespace filter {

struct callbacks;
using backend_t = eosio::vm::backend<callbacks, eosio::vm::jit>;
using rhf_t     = eosio::vm::registered_host_functions<callbacks>;

struct filter_state : history_tools::data_state<backend_t>, history_tools::console_state, history_tools::filter_callback_state {
   eosio::vm::wasm_allocator wa = {};
};

// todo: remove basic_callbacks
struct callbacks : history_tools::basic_callbacks<callbacks>,
                   history_tools::chaindb_callbacks<callbacks>,
                   history_tools::compiler_builtins_callbacks<callbacks>,
                   history_tools::console_callbacks<callbacks>,
                   history_tools::data_callbacks<callbacks>,
                   history_tools::db_callbacks<callbacks>,
                   history_tools::filter_callbacks<callbacks>,
                   history_tools::memory_callbacks<callbacks>,
                   history_tools::unimplemented_callbacks<callbacks>,
                   history_tools::unimplemented_filter_callbacks<callbacks> {
   filter::filter_state&         filter_state;
   history_tools::chaindb_state& chaindb_state;
   history_tools::db_view_state& db_view_state;

   callbacks(filter::filter_state& filter_state, history_tools::chaindb_state& chaindb_state,
             history_tools::db_view_state& db_view_state)
       : filter_state{ filter_state }, chaindb_state{ chaindb_state }, db_view_state{ db_view_state } {}

   auto& get_state() { return filter_state; }
   auto& get_filter_callback_state() { return filter_state; }
   auto& get_chaindb_state() { return chaindb_state; }
   auto& get_db_view_state() { return db_view_state; }
};

inline void register_callbacks() {
   history_tools::basic_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
   history_tools::chaindb_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
   history_tools::compiler_builtins_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
   history_tools::console_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
   history_tools::data_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
   history_tools::db_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
   history_tools::filter_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
   history_tools::memory_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
   history_tools::unimplemented_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
   history_tools::unimplemented_filter_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
}

}}} // namespace eosio::history_tools::filter
