#include "table.hpp"

struct contract_table_kv : table<contract_table> {
    index primary_index{"primary"_n, [](const auto& var) {
                            return std::visit(
                                [](const auto& obj) { return abieos::native_to_key(std::tie(obj.code, obj.table, obj.scope)); }, var);
                        }};

    contract_table_kv() { init("system"_n, "contract.row"_n, primary_index); }
};

struct contract_row_kv : table<contract_row> {
    index primary_index{
        "primary"_n, [](const auto& var) {
            return std::visit(
                [](const auto& obj) { return abieos::native_to_key(std::tie(obj.code, obj.table, obj.scope, obj.primary_key)); }, var);
        }};

    contract_row_kv() { init("system"_n, "contract.row"_n, primary_index); }
};

template <typename Table>
void process_delta(state_history::table_delta_v0& delta) {
    Table table;
    for (auto& row : delta.rows) {
        auto obj = assert_bin_to_native<typename Table::value_type>(row.data);
        if (row.present)
            table.insert(obj);
        else
            table.erase(obj);
    }
}

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
        switch (abieos::string_to_name(d.name.c_str())) {

        case "contract_table"_n.value: process_delta<contract_table_kv>(d); break;
        case "contract_row"_n.value: process_delta<contract_row_kv>(d); break;
        }
    }
}
