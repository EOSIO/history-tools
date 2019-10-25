#include "handle_action.hpp"
#include "state_history_kv_tables.hpp"

extern "C" __attribute__((eosio_wasm_entry)) void initialize() {}

extern "C" __attribute__((eosio_wasm_entry)) void start() {
    auto res = std::get<get_blocks_result_v0>(state_history::assert_bin_to_native<result>(get_bin()));
    if (!res.this_block || !res.traces || !res.deltas)
        return;
    auto traces = state_history::assert_bin_to_native<std::vector<transaction_trace>>(*res.traces);
    auto deltas = state_history::assert_bin_to_native<std::vector<table_delta>>(*res.deltas);
    print("block ", res.this_block->block_num, " traces: ", traces.size(), " deltas: ", deltas.size(), "\n");
    state_history::store_deltas(deltas);
}
