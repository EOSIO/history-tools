#include "state_history_connection.hpp"
#include <abieos_exception.hpp>
#include <eosio/vm/backend.hpp>
#include <fc/exception/exception.hpp>

struct callbacks;
using backend_t = eosio::vm::backend<callbacks, eosio::vm::jit>;
using rhf_t     = eosio::vm::registered_host_functions<callbacks>;

struct assert_exception : std::exception {
    std::string msg;

    assert_exception(std::string&& msg)
        : msg(std::move(msg)) {}

    const char* what() const noexcept override { return msg.c_str(); }
};

namespace eosio {
namespace vm {

template <>
struct wasm_type_converter<const char*> : linear_memory_access {
    auto from_wasm(void* ptr) { return (const char*)ptr; }
};

template <>
struct wasm_type_converter<char*> : linear_memory_access {
    auto from_wasm(void* ptr) { return (char*)ptr; }
};

template <typename T>
struct wasm_type_converter<T&> : linear_memory_access {
    auto from_wasm(uint32_t val) {
        EOS_VM_ASSERT(val != 0, wasm_memory_exception, "references cannot be created for null pointers");
        void* ptr = get_ptr(val);
        validate_ptr<T>(ptr, 1);
        return eosio::vm::aligned_ref_wrapper<T, alignof(T)>{ptr};
    }
};

} // namespace vm
} // namespace eosio

struct state {
    const char*                wasm;
    eosio::vm::wasm_allocator& wa;
    backend_t&                 backend;
    std::vector<char>          args;
    abieos::input_buffer       bin;

    state(const char* wasm, eosio::vm::wasm_allocator& wa, backend_t& backend, std::vector<char> args)
        : wasm{wasm}
        , wa{wa}
        , backend{backend}
        , args{args} {}
};

struct callbacks {
    ::state& state;

    void check_bounds(const char* begin, const char* end) {
        if (begin > end)
            throw std::runtime_error("bad memory");
        // todo: check bounds
    }

    // char* alloc(uint32_t cb_alloc_data, uint32_t cb_alloc, uint32_t size) {
    //     // todo: verify cb_alloc isn't in imports
    //     auto result = state.backend.get_context().execute_func_table(
    //         this, eosio::vm::interpret_visitor(state.backend.get_context()), cb_alloc, cb_alloc_data, size);
    //     if (!result || !result->is_a<eosio::vm::i32_const_t>())
    //         throw std::runtime_error("cb_alloc returned incorrect type");
    //     char* begin = state.wa.get_base_ptr<char>() + result->to_ui32();
    //     check_bounds(begin, begin + size);
    //     return begin;
    // }

    // template <typename T>
    // void set_data(uint32_t cb_alloc_data, uint32_t cb_alloc, const T& data) {
    //     memcpy(alloc(cb_alloc_data, cb_alloc, data.size()), data.data(), data.size());
    // }

    void abort() { throw std::runtime_error("called abort"); }

    void eosio_assert_message(bool test, const char* msg, size_t msg_len) {
        check_bounds(msg, msg + msg_len);
        if (!test)
            throw ::assert_exception(std::string(msg, msg_len));
    }

    void print_range(const char* begin, const char* end) {
        check_bounds(begin, end);
        std::cerr.write(begin, end - begin);
    }

    uint32_t get_bin_size() { return state.bin.end - state.bin.pos; }

    void get_bin(char* dest_begin, char* dest_end) {
        check_bounds(dest_begin, dest_end);
        if (dest_end - dest_begin < state.bin.end - state.bin.pos)
            throw std::runtime_error("get_bin: dest buffer too small");
        memcpy(dest_begin, state.bin.pos, state.bin.end - state.bin.pos);
    }

    // void get_args(uint32_t cb_alloc_data, uint32_t cb_alloc) { set_data(cb_alloc_data, cb_alloc, state.args); }
}; // callbacks

void register_callbacks() {
    rhf_t::add<callbacks, &callbacks::abort, eosio::vm::wasm_allocator>("env", "abort");
    rhf_t::add<callbacks, &callbacks::eosio_assert_message, eosio::vm::wasm_allocator>("env", "eosio_assert_message");
    rhf_t::add<callbacks, &callbacks::print_range, eosio::vm::wasm_allocator>("env", "print_range");
    rhf_t::add<callbacks, &callbacks::get_bin_size, eosio::vm::wasm_allocator>("env", "get_bin_size");
    rhf_t::add<callbacks, &callbacks::get_bin, eosio::vm::wasm_allocator>("env", "get_bin");
}

struct ship_connection_state : state_history::connection_callbacks, std::enable_shared_from_this<ship_connection_state> {
    std::shared_ptr<::state>                   state;
    std::shared_ptr<state_history::connection> connection;

    ship_connection_state(const std::shared_ptr<::state>& state)
        : state(state) {}

    void start(boost::asio::io_context& ioc) {
        state_history::connection_config config{"127.0.0.1", "8080"};
        connection = std::make_shared<state_history::connection>(ioc, config, shared_from_this());
        connection->connect();
    }

    void received_abi(std::string_view abi) override {
        ilog("received_abi");
        connection->send(state_history::get_status_request_v0{});
    }

    bool received(state_history::get_status_result_v0& status, abieos::input_buffer bin) override {
        ilog("received status");
        connection->request_blocks(status, 0, {});
        return true;
    }

    bool received(state_history::get_blocks_result_v0& result, abieos::input_buffer bin) override {
        ilog("received block ${n}", ("n", result.this_block ? result.this_block->block_num : -1));
        callbacks cb{*state};
        state->bin = bin;
        state->backend.initialize(&cb);
        // backend(&cb, "env", "initialize"); // todo: needs change to eosio-cpp
        state->backend(&cb, "env", "start", 0);
        return true;
    }

    void closed(bool retry) override { ilog("closed"); }
};

static void run(const char* wasm, const std::vector<std::string>& args) {
    eosio::vm::wasm_allocator wa;
    auto                      code = backend_t::read_wasm(wasm);
    backend_t                 backend(code);
    auto                      state = std::make_shared<::state>(wasm, wa, backend, abieos::native_to_bin(args));
    //callbacks                 cb{*state};
    backend.set_wasm_allocator(&wa);

    rhf_t::resolve(backend.get_module());

    boost::asio::io_context          ioc;
    state_history::connection_config config{"127.0.0.1", "8080"};
    auto                             ship_state = std::make_shared<ship_connection_state>(state);
    ship_state->start(ioc);
    ioc.run();
}

const char usage[] = "usage: eosio-tester [-h or --help] [-v or --verbose] file.wasm [args for wasm]\n";

int main(int argc, char* argv[]) {
    fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::off);

    bool show_usage = false;
    bool error      = false;
    int  next_arg   = 1;
    while (next_arg < argc && argv[next_arg][0] == '-') {
        if (!strcmp(argv[next_arg], "-h") || !strcmp(argv[next_arg], "--help"))
            show_usage = true;
        else if (!strcmp(argv[next_arg], "-v") || !strcmp(argv[next_arg], "--verbose"))
            fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);
        else {
            std::cerr << "unknown option: " << argv[next_arg] << "\n";
            error = true;
        }
        ++next_arg;
    }
    if (next_arg >= argc)
        error = true;
    if (show_usage || error) {
        std::cerr << usage;
        return error;
    }
    try {
        std::vector<std::string> args{argv + next_arg + 1, argv + argc};
        register_callbacks();
        run(argv[next_arg], args);
        return 0;
    } catch (eosio::vm::exception& e) {
        std::cerr << "vm::exception: " << e.detail() << "\n";
    } catch (::assert_exception& e) {
        std::cerr << "assert failed: " << e.what() << "\n";
    } catch (std::exception& e) {
        std::cerr << "std::exception: " << e.what() << "\n";
    } catch (fc::exception& e) {
        std::cerr << "fc::exception: " << e.to_string() << "\n";
    }
    return 1;
}
