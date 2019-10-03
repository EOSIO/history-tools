#include "../src/state_history.hpp"
#include <eosio/check.hpp>
#include <eosio/print.hpp>
#include <vector>

using namespace eosio;
using namespace state_history;
using namespace abieos::literals;

namespace eosio {
namespace internal_use_do_not_use {

extern "C" __attribute__((eosio_wasm_import)) void get_bin(void* cb_alloc_data, void* (*cb_alloc)(void* cb_alloc_data, size_t size));
extern "C" __attribute__((eosio_wasm_import)) void set_kv(const char* k_begin, const char* k_end, const char* v_begin, const char* v_end);

template <typename Alloc_fn>
inline void get_bin(Alloc_fn alloc_fn) {
    return get_bin(&alloc_fn, [](void* cb_alloc_data, size_t size) -> void* { //
        return (*reinterpret_cast<Alloc_fn*>(cb_alloc_data))(size);
    });
}

} // namespace internal_use_do_not_use

inline const std::vector<char>& get_bin() {
    static std::optional<std::vector<char>> bytes;
    if (!bytes) {
        internal_use_do_not_use::get_bin([&](size_t size) {
            bytes.emplace();
            bytes->resize(size);
            return bytes->data();
        });
    }
    return *bytes;
}

struct handle_action_base {
    abieos::name contract;
    abieos::name action;

    virtual void dispatch(abieos::input_buffer bin) = 0;

    static std::vector<handle_action_base*>& get_actions() {
        static std::vector<handle_action_base*> actions;
        return actions;
    }
};

template <typename T>
struct serial_dispatcher;

template <typename C, typename... Args>
struct serial_dispatcher<void (C::*)(Args...) const> {
    using type = std::tuple<Args...>;

    template <typename F>
    static void dispatch(F f, abieos::input_buffer bin) {
        std::apply(f, std::tuple<std::decay_t<Args>...>{state_history::assert_bin_to_native<std::decay_t<Args>>(bin)...});
    }
};

template <typename F>
struct handle_action : handle_action_base {
    F f;

    handle_action(abieos::name contract, abieos::name action, F f)
        : f(f) {
        this->contract = contract;
        this->action   = action;
        get_actions().push_back(this);
    }
    handle_action(const handle_action&) = delete;
    handle_action& operator=(const handle_action&) = delete;

    void dispatch(abieos::input_buffer bin) override { serial_dispatcher<decltype(&F::operator())>::dispatch(f, bin); }
};

} // namespace eosio

extern "C" __attribute__((eosio_wasm_entry)) void initialize() {}

extern "C" __attribute__((eosio_wasm_entry)) void start() {
    auto res = std::get<get_blocks_result_v0>(assert_bin_to_native<result>(get_bin()));
    if (!res.this_block || !res.traces || !res.deltas)
        return;
    auto traces = assert_bin_to_native<std::vector<transaction_trace>>(*res.traces);
    auto deltas = assert_bin_to_native<std::vector<table_delta>>(*res.deltas);
    print("block ", res.this_block->block_num, " traces: ", traces.size(), " deltas: ", deltas.size(), "\n");
    for (auto& trace : traces) {
        auto& t = std::get<transaction_trace_v0>(trace);
        // print("    trace: status: ", to_string(t.status), " action_traces: ", t.action_traces.size(), "\n");
        if (t.status != state_history::transaction_status::executed)
            continue;
        for (auto& atrace : t.action_traces) {
            auto& at = std::get<action_trace_v0>(atrace);
            if (at.receiver != at.act.account)
                continue;
            for (auto& handler : handle_action_base::get_actions()) {
                if (at.receiver == handler->contract && at.act.name == handler->action) {
                    handler->dispatch(at.act.data);
                    break;
                }
            }
            // if (at.receiver == "eosio.token"_n && at.act.name == "transfer"_n) {
            //     print("transfer\n");
            // }
        }
    }
    // for (auto& delta : deltas) {
    //     auto& d = std::get<table_delta_v0>(delta);
    //     print("    table: ", d.name, " rows: ", d.rows.size(), "\n");
    // }
}

/////////////////////////////////////////

auto token_transfer = eosio::handle_action(
    "eosio.token"_n, "transfer"_n, [](abieos::name from, abieos::name to, const abieos::asset& quantity, const std::string& memo) {
        print("    transfer ", (std::string)from, " ", (std::string)to, " ", asset_to_string(quantity), " ", memo, "\n");
    });

auto eosio_buyrex = eosio::handle_action("eosio"_n, "buyrex"_n, [](abieos::name from, const abieos::asset& amount) {
    print("    buyrex ", (std::string)from, " ", asset_to_string(amount), "\n");
});

auto eosio_sellrex = eosio::handle_action("eosio"_n, "sellrex"_n, [](abieos::name from, const abieos::asset& rex) {
    print("    sellrex ", (std::string)from, " ", asset_to_string(rex), "\n");
});
