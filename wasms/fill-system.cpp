#include "table.hpp"

extern "C" __attribute__((eosio_wasm_entry)) void initialize() {}

extern "C" __attribute__((eosio_wasm_entry)) void start() {
    auto res = std::get<get_blocks_result_v0>(assert_bin_to_native<result>(get_bin()));
    if (!res.this_block || !res.traces || !res.deltas)
        return;
    auto traces = assert_bin_to_native<std::vector<transaction_trace>>(*res.traces);
    auto deltas = assert_bin_to_native<std::vector<table_delta>>(*res.deltas);
    print("block ", res.this_block->block_num, " traces: ", traces.size(), " deltas: ", deltas.size(), "\n");
    for (auto& delta : deltas) {
        auto& d = std::get<table_delta_v0>(delta);
        print("    table: ", d.name, " rows: ", d.rows.size(), "\n");
    }
}

/////////////////////////////////////////

struct transfer_data {
    uint64_t     recv_sequence = {};
    eosio::name  from          = {};
    eosio::name  to            = {};
    eosio::asset quantity      = {};
    std::string  memo          = {};

    auto primary_key() const { return recv_sequence; }
    auto from_key() const { return std::tie(from, recv_sequence); }
    auto to_key() const { return std::tie(to, recv_sequence); }
};

ABIEOS_REFLECT(transfer_data) {
    ABIEOS_MEMBER(transfer_data, recv_sequence);
    ABIEOS_MEMBER(transfer_data, from);
    ABIEOS_MEMBER(transfer_data, to);
    ABIEOS_MEMBER(transfer_data, quantity);
    ABIEOS_MEMBER(transfer_data, memo);
}

struct transfer_history : table<transfer_data> {
    index primary_index{"primary"_n, [](const auto& obj) { return abieos::native_to_key(obj.primary_key()); }};
    index from_index{"from"_n, [](const auto& obj) { return abieos::native_to_key(obj.from_key()); }};
    index to_index{"to"_n, [](const auto& obj) { return abieos::native_to_key(obj.to_key()); }};

    transfer_history() { init("my.context"_n, "my.table"_n, primary_index, from_index, to_index); }
};

transfer_history xfer_hist;

eosio::handle_action token_transfer(
    "eosio.token"_n, "transfer"_n,
    [](const action_context& context, eosio::name from, eosio::name to, const eosio::asset& quantity, const std::string& memo) {
        // print(
        //     "    ", std::get<0>(*std::get<0>(context.atrace).receipt).recv_sequence, " transfer ", from, " ", to, " ", quantity, " ", memo,
        //     "\n");
        xfer_hist.insert({std::get<0>(*std::get<0>(context.atrace).receipt).recv_sequence, from, to, quantity, memo});
    });

eosio::handle_action eosio_buyrex("eosio"_n, "buyrex"_n, [](const action_context& context, eosio::name from, const eosio::asset& amount) {
    print("    buyrex ", from, " ", amount, "\n");

    print("    ===== sequence\n");
    int i = 0;
    for (auto prox : xfer_hist) {
        print("        ", i, ": ", prox.get().recv_sequence, " ", prox.get().from, " ", prox.get().to, " ", prox.get().quantity, "\n");
        ++i;
    }

    print("    ===== from\n");
    int i2 = 0;
    for (auto prox : xfer_hist.from_index) {
        print("        ", i2, ": ", prox.get().from, " ", prox.get().to, " ", prox.get().quantity, "\n");
        ++i2;
    }

    print("    ===== to\n");
    int i3 = 0;
    for (auto prox : xfer_hist.to_index) {
        print("        ", i3, ": ", prox.get().from, " ", prox.get().to, " ", prox.get().quantity, "\n");
        ++i3;
    }
});

// eosio::handle_action eosio_sellrex("eosio"_n, "sellrex"_n, [](const action_context& context, eosio::name from, const eosio::asset& rex) { //
//     print("    sellrex ", from, " ", rex, "\n");
// });
