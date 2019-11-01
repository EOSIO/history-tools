// copyright defined in LICENSE.txt

#include "wasm_ql.hpp"
#include "abieos_exception.hpp"
#include "chaindb_callbacks.hpp"

#include <fc/log/logger.hpp>
#include <fc/scoped_exit.hpp>

using namespace abieos::literals;

namespace wasm_ql {

struct callbacks;
using backend_t = eosio::vm::backend<callbacks, eosio::vm::jit>;
using rhf_t     = eosio::vm::registered_host_functions<callbacks>;

struct callbacks : history_tools::basic_callbacks<callbacks>,
                   history_tools::data_callbacks<callbacks>,
                   history_tools::chaindb_callbacks<callbacks> {
    wasm_ql::thread_state&             thread_state;
    history_tools::chaindb_state&      chaindb_state;
    state_history::rdb::db_view_state& db_view_state;

    callbacks(
        wasm_ql::thread_state& thread_state, history_tools::chaindb_state& chaindb_state, state_history::rdb::db_view_state& db_view_state)
        : thread_state{thread_state}
        , chaindb_state{chaindb_state}
        , db_view_state{db_view_state} {}

    auto& get_state() { return thread_state; }
    auto& get_chaindb_state() { return chaindb_state; }
    auto& get_db_view_state() { return db_view_state; }
};

void register_callbacks() {
    history_tools::basic_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
    history_tools::data_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
    history_tools::chaindb_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
}

/*
static void fill_context_data(wasm_ql::thread_state& thread_state) {
    thread_state.database_status.clear();
    abieos::native_to_bin(thread_state.fill_status.head, thread_state.database_status);
    abieos::native_to_bin(thread_state.fill_status.head_id, thread_state.database_status);
    abieos::native_to_bin(thread_state.fill_status.irreversible, thread_state.database_status);
    abieos::native_to_bin(thread_state.fill_status.irreversible_id, thread_state.database_status);
    abieos::native_to_bin(thread_state.fill_status.first, thread_state.database_status);
}
*/

static void run_query(wasm_ql::thread_state& thread_state, abieos::name short_name, abieos::name query_name) {
    auto                        code = backend_t::read_wasm(thread_state.shared->wasm_dir + "/" + (std::string)short_name + ".query.wasm");
    backend_t                   backend(code);
    state_history::rdb::db_view view{*thread_state.shared->db};
    state_history::rdb::db_view_state db_view_state{view};
    history_tools::chaindb_state      chaindb_state;
    callbacks                         cb{thread_state, chaindb_state, db_view_state};
    backend.set_wasm_allocator(&thread_state.wa);

    // todo: make short_name available to wasm
    rhf_t::resolve(backend.get_module());
    backend.initialize(&cb);
    backend(&cb, "env", "initialize");
    backend(&cb, "env", "run_query", short_name.value, query_name.value);
}

const std::vector<char>&
query(wasm_ql::thread_state& thread_state, std::string_view wasm, std::string_view query, const std::vector<char>& request) {
    abieos::name wasm_name;
    if (!abieos::string_to_name_strict(wasm, wasm_name.value))
        throw std::runtime_error("invalid wasm name");
    abieos::name query_name;
    if (!abieos::string_to_name_strict(query, query_name.value))
        throw std::runtime_error("invalid query name");
    thread_state.input_data = abieos::input_buffer{request.data(), request.data() + request.size()};
    run_query(thread_state, wasm_name, query_name);
    return thread_state.output_data;
}

const std::vector<char>& legacy_query(wasm_ql::thread_state& thread_state, const std::string& target, const std::vector<char>& request) {
    std::vector<char> req;
    abieos::native_to_bin(target, req);
    abieos::native_to_bin(request, req);
    thread_state.input_data = abieos::input_buffer{req.data(), req.data() + req.size()};
    // run_query(thread_state, "legacy"_n);
    return thread_state.output_data;
}

} // namespace wasm_ql
