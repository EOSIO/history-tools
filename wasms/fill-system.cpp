#include "handle_action.hpp"
#include "state_history_kv_tables.hpp"

extern "C" __attribute__((eosio_wasm_entry)) void initialize() {}

extern "C" __attribute__((eosio_wasm_entry)) void start() {
    auto res = std::get<get_blocks_result_v0>(eosio::check(eosio::convert_from_bin<state_history::result>(get_bin())).value());
    if (!res.this_block || !res.traces || !res.deltas)
        return;
    auto traces = eosio::check(eosio::from_bin<std::vector<transaction_trace>>(*res.traces)).value();
    auto deltas = eosio::check(eosio::from_bin<std::vector<table_delta>>(*res.deltas)).value();
    print("block ", res.this_block->block_num, " traces: ", traces.size(), " deltas: ", deltas.size(), "\n");
    state_history::store_deltas({}, deltas, false);
}
