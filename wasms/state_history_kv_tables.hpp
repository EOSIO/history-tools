#include "../src/state_history.hpp"
#include "table.hpp"

namespace state_history {

struct contract_table_kv : eosio::table<contract_table> {
    index primary_index{abieos::name{"primary"}, [](const auto& var) {
                            return std::visit(
                                [](const auto& obj) { return abieos::native_to_key(std::tie(obj.code, obj.table, obj.scope)); }, var);
                        }};

    contract_table_kv() { init(abieos::name{"system"}, abieos::name{"contract.tab"}, primary_index); }
};

struct contract_row_kv : eosio::table<contract_row> {
    index primary_index{
        abieos::name{"primary"}, [](const auto& var) {
            return std::visit(
                [](const auto& obj) { return abieos::native_to_key(std::tie(obj.code, obj.table, obj.scope, obj.primary_key)); }, var);
        }};

    contract_row_kv() { init(abieos::name{"system"}, abieos::name{"contract.row"}, primary_index); }
};

template <typename Table>
void store_delta(table_delta_v0& delta) {
    Table table;
    for (auto& row : delta.rows) {
        auto obj = assert_bin_to_native<typename Table::value_type>(row.data);
        if (row.present)
            table.insert(obj);
        else
            table.erase(obj);
    }
}

inline void store_deltas(std::vector<table_delta>& deltas) {
    for (auto& delta : deltas) {
        auto& d = std::get<table_delta_v0>(delta);
        if (d.name == "contract_table")
            store_delta<contract_table_kv>(d);
        if (d.name == "contract_row")
            store_delta<contract_row_kv>(d);
    }
}

} // namespace state_history
