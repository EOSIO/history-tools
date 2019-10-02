#include "../src/state_history.hpp"
#include <eosio/check.hpp>
#include <eosio/print.hpp>
#include <vector>

using namespace eosio;
using namespace state_history;

namespace eosio {
namespace internal_use_do_not_use {

extern "C" __attribute__((eosio_wasm_import)) void get_bin(void* cb_alloc_data, void* (*cb_alloc)(void* cb_alloc_data, size_t size));

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

} // namespace eosio

extern "C" __attribute__((eosio_wasm_entry)) void initialize() {}

extern "C" __attribute__((eosio_wasm_entry)) void start() {
    auto res = std::get<get_blocks_result_v0>(assert_bin_to_native<result>(get_bin()));
    if (!res.this_block || !res.traces || !res.deltas)
        return;
    auto traces = assert_bin_to_native<std::vector<transaction_trace>>(*res.traces);
    auto deltas = assert_bin_to_native<std::vector<table_delta>>(*res.deltas);
    print("block ", res.this_block->block_num, " traces: ", traces.size(), " deltas: ", deltas.size(), "\n");
    // for (auto& trace : traces) {
    //     auto& t = std::get<transaction_trace_v0>(trace);
    //     print("    trace: status: ", to_string(t.status), " action_traces: ", t.action_traces.size(), "\n");
    // }
    // for (auto& delta : deltas) {
    //     auto& d = std::get<table_delta_v0>(delta);
    //     print("    table: ", d.name, " rows: ", d.rows.size(), "\n");
    // }
}
