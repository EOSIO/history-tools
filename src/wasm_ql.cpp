// copyright defined in LICENSE.txt

#include "wasm_ql.hpp"
#include "abieos_exception.hpp"

#include <fc/log/logger.hpp>
#include <fc/scoped_exit.hpp>

using namespace abieos::literals;

namespace wasm_ql {

struct callbacks;
using backend_t = eosio::vm::backend<callbacks>;
using rhf_t     = eosio::vm::registered_host_functions<callbacks>;

struct callbacks {
    wasm_ql::thread_state& thread_state;
    backend_t&             backend;

    void check_bounds(const char* begin, const char* end) {
        if (begin > end)
            throw std::runtime_error("bad memory");
        // todo: check bounds
    }

    char* alloc(uint32_t cb_alloc_data, uint32_t cb_alloc, uint32_t size) {
        // todo: verify cb_alloc isn't in imports
        auto result = backend.get_context().execute_func_table(
            this, eosio::vm::interpret_visitor(backend.get_context()), cb_alloc, cb_alloc_data, size);
        if (!result || !result->is_a<eosio::vm::i32_const_t>())
            throw std::runtime_error("cb_alloc returned incorrect type");
        char* begin = thread_state.wa.get_base_ptr<char>() + result->to_ui32();
        check_bounds(begin, begin + size);
        return begin;
    }

    void abort() { throw std::runtime_error("called abort"); }

    void eosio_assert_message(bool test, const char* msg, size_t msg_len) {
        // todo: pass assert message through RPC API
        if (!test)
            throw std::runtime_error("assert failed");
    }

    /*
    void get_database_status(uint32_t cb_alloc_data, uint32_t cb_alloc) {
        auto data = alloc(cb_alloc_data, cb_alloc, thread_state.database_status.size());
        memcpy(data, thread_state.database_status.data(), thread_state.database_status.size());
    }
    */

    void get_input_data(uint32_t cb_alloc_data, uint32_t cb_alloc) {
        auto data = alloc(cb_alloc_data, cb_alloc, thread_state.request.end - thread_state.request.pos);
        memcpy(data, thread_state.request.pos, thread_state.request.end - thread_state.request.pos);
    }

    void set_output_data(const char* begin, const char* end) {
        check_bounds(begin, end);
        thread_state.reply.assign(begin, end);
    }

    /*
    void query_database(const char* req_begin, const char* req_end, uint32_t cb_alloc_data, uint32_t cb_alloc) {
        check_bounds(req_begin, req_end);
        auto result = thread_state.query_session->query_database({req_begin, req_end}, thread_state.fill_status.head);
        auto data   = alloc(cb_alloc_data, cb_alloc, result.size());
        memcpy(data, result.data(), result.size());
    }
    */

    void print_range(const char* begin, const char* end) {
        check_bounds(begin, end);
        if (thread_state.shared->console)
            std::cerr.write(begin, end - begin);
    }
}; // callbacks

void register_callbacks() {
    /*
    rhf_t::add<callbacks, &callbacks::abort, eosio::vm::wasm_allocator>("env", "abort");
    rhf_t::add<callbacks, &callbacks::eosio_assert_message, eosio::vm::wasm_allocator>("env", "eosio_assert_message");
    rhf_t::add<callbacks, &callbacks::get_database_status, eosio::vm::wasm_allocator>("env", "get_database_status");
    rhf_t::add<callbacks, &callbacks::get_input_data, eosio::vm::wasm_allocator>("env", "get_input_data");
    rhf_t::add<callbacks, &callbacks::set_output_data, eosio::vm::wasm_allocator>("env", "set_output_data");
    rhf_t::add<callbacks, &callbacks::query_database, eosio::vm::wasm_allocator>("env", "query_database");
    rhf_t::add<callbacks, &callbacks::print_range, eosio::vm::wasm_allocator>("env", "print_range");
    */
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
    auto      code = backend_t::read_wasm(thread_state.shared->wasm_dir + "/" + (std::string)short_name + "-server.wasm");
    backend_t backend(code);
    callbacks cb{thread_state, backend};
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
        thread_state.request = abieos::bin_to_native<abieos::input_buffer>(request_bin);
        auto ns_name         = abieos::bin_to_native<abieos::name>(thread_state.request);
        if (ns_name != "local"_n)
            throw std::runtime_error("unknown namespace: " + (std::string)ns_name);
        auto short_name = abieos::bin_to_native<abieos::name>(thread_state.request);

        run_query(thread_state, short_name);

        // elog("result: ${s} ${x}", ("s", thread_state.reply.size())("x", fc::to_hex(thread_state.reply)));
        abieos::push_varuint32(result, thread_state.reply.size());
        result.insert(result.end(), thread_state.reply.begin(), thread_state.reply.end());
    }
    return result;
}

const std::vector<char>& legacy_query(wasm_ql::thread_state& thread_state, const std::string& target, const std::vector<char>& request) {
    std::vector<char> req;
    abieos::native_to_bin(target, req);
    abieos::native_to_bin(request, req);
    thread_state.request = abieos::input_buffer{req.data(), req.data() + req.size()};
    run_query(thread_state, "legacy"_n);
    return thread_state.reply;
}

} // namespace wasm_ql
