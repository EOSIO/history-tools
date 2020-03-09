#include "state_history_connection.hpp"
#include <eosio/history-tools/callbacks/kv.hpp>
#include <eosio/history-tools/callbacks/basic.hpp>
#include <fc/exception/exception.hpp>

struct callbacks;
using backend_t = eosio::vm::backend<callbacks, eosio::vm::jit>;
using rhf_t     = eosio::vm::registered_host_functions<callbacks>;

struct state : history_tools::wasm_state<backend_t>, state_history::rdb::db_view_state {
    std::vector<char>    args;
    abieos::input_buffer bin;

    state(const char* wasm, eosio::vm::wasm_allocator& wa, backend_t& backend, chain_kv::database& db, std::vector<char> args)
        : wasm_state{wa, backend}
        , db_view_state{db}
        , args{args} {}
};

struct callbacks : history_tools::basic_callbacks<callbacks>, state_history::rdb::db_callbacks<callbacks> {
    ::state& state;

    callbacks(::state& state)
        : state{state} {}

    void get_bin(uint32_t cb_alloc_data, uint32_t cb_alloc) { set_data(cb_alloc_data, cb_alloc, state.bin); }
    void get_args(uint32_t cb_alloc_data, uint32_t cb_alloc) { set_data(cb_alloc_data, cb_alloc, state.args); }
}; // callbacks

void register_callbacks() {
    history_tools::basic_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
    state_history::rdb::db_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();

    rhf_t::add<callbacks, &callbacks::get_bin, eosio::vm::wasm_allocator>("env", "get_bin");
    rhf_t::add<callbacks, &callbacks::get_args, eosio::vm::wasm_allocator>("env", "get_args");
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
        state->reset();
        state->bin = bin;
        state->backend.initialize(&cb);
        // backend(&cb, "env", "initialize"); // todo: needs change to eosio-cpp
        state->backend(&cb, "env", "start", 0);
        state->write_and_reset();
        return true;
    }

    void closed(bool retry) override { ilog("closed"); }
};

static void run(const char* wasm, const std::vector<std::string>& args) {
    eosio::vm::wasm_allocator wa;
    auto                      code = backend_t::read_wasm(wasm);
    backend_t                 backend(code);
    chain_kv::database        db{"db.rocksdb", {}, {}, true};
    auto                      state = std::make_shared<::state>(wasm, wa, backend, db, abieos::native_to_bin(args));
    // todo: state: drop shared_ptr?
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
    fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::error);

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
    } catch (history_tools::assert_exception& e) {
        std::cerr << "assert failed: " << e.what() << "\n";
    } catch (std::exception& e) {
        std::cerr << "std::exception: " << e.what() << "\n";
    } catch (fc::exception& e) {
        std::cerr << "fc::exception: " << e.to_string() << "\n";
    }
    return 1;
}
