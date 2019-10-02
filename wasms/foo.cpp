#include "../src/state_history.hpp"
#include <eosio/check.hpp>
#include <eosio/print.hpp>
#include <vector>

using namespace eosio;
using namespace state_history;

extern "C" __attribute__((eosio_wasm_import)) uint32_t get_bin_size();
extern "C" __attribute__((eosio_wasm_import)) void     get_bin(char* dest_begin, char* dest_end);

extern "C" __attribute__((eosio_wasm_entry)) void initialize() {}

extern "C" __attribute__((eosio_wasm_entry)) void start() {
    std::vector<char> bin(get_bin_size());
    get_bin(bin.data(), bin.data() + bin.size());
    auto res = std::get<get_blocks_result_v0>(assert_bin_to_native<result>(bin));
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
