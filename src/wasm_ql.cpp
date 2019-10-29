// copyright defined in LICENSE.txt

#include "wasm_ql.hpp"
#include "abieos_exception.hpp"

#include <fc/log/logger.hpp>
#include <fc/scoped_exit.hpp>

using namespace abieos::literals;

namespace wasm_ql {

struct callbacks;
using backend_t = eosio::vm::backend<callbacks, eosio::vm::jit>;
using rhf_t     = eosio::vm::registered_host_functions<callbacks>;

struct callbacks : history_tools::basic_callbacks<callbacks>, history_tools::data_callbacks<callbacks> {
    wasm_ql::thread_state& thread_state;

    auto& get_state() { return thread_state; }

    callbacks(wasm_ql::thread_state& thread_state)
        : thread_state{thread_state} {}
}; // callbacks

void register_callbacks() {
    history_tools::basic_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
    history_tools::data_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
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

static void run_query(wasm_ql::thread_state& thread_state, abieos::name short_name) {
    auto      code = backend_t::read_wasm(thread_state.shared->wasm_dir + "/" + (std::string)short_name + ".query.wasm");
    backend_t backend(code);
    callbacks cb{thread_state};
    backend.set_wasm_allocator(&thread_state.wa);

    rhf_t::resolve(backend.get_module());
    backend.initialize(&cb);
    backend(&cb, "env", "initialize");
    backend(&cb, "env", "run_query");
}

std::vector<char> query(wasm_ql::thread_state& thread_state, const std::vector<char>& request) {
    std::vector<char>    result;
    abieos::input_buffer request_bin{request.data(), request.data() + request.size()};
    auto                 num_requests = abieos::bin_to_native<abieos::varuint32>(request_bin).value;
    result.clear();
    abieos::push_varuint32(result, num_requests);
    for (uint32_t request_index = 0; request_index < num_requests; ++request_index) {
        thread_state.input_data = abieos::bin_to_native<abieos::input_buffer>(request_bin);
        auto ns_name            = abieos::bin_to_native<abieos::name>(thread_state.input_data);
        if (ns_name != "local"_n)
            throw std::runtime_error("unknown namespace: " + (std::string)ns_name);
        auto short_name = abieos::bin_to_native<abieos::name>(thread_state.input_data);

        run_query(thread_state, short_name);

        // elog("result: ${s} ${x}", ("s", thread_state.output_data.size())("x", fc::to_hex(thread_state.output_data)));
        abieos::push_varuint32(result, thread_state.output_data.size());
        result.insert(result.end(), thread_state.output_data.begin(), thread_state.output_data.end());
    }
    return result;
}

const std::vector<char>& legacy_query(wasm_ql::thread_state& thread_state, const std::string& target, const std::vector<char>& request) {
    std::vector<char> req;
    abieos::native_to_bin(target, req);
    abieos::native_to_bin(request, req);
    thread_state.input_data = abieos::input_buffer{req.data(), req.data() + req.size()};
    run_query(thread_state, "legacy"_n);
    return thread_state.output_data;
}

} // namespace wasm_ql
