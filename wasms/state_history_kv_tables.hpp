#include "../src/state_history.hpp"
#include "table.hpp"
#include <eosio/to_key.hpp>

namespace state_history {

struct fill_status_kv : eosio::table<fill_status> {
    index primary_index{abieos::name{"primary"}, [](const auto& var) { return std::vector<char>{}; }};

    fill_status_kv(eosio::kv_environment environment)
        : eosio::table<fill_status>{std::move(environment)} {
        init(abieos::name{"eosio.kvdisk"}, abieos::name{"system"}, abieos::name{"fill.status"}, primary_index);
    }
};

struct contract_table_kv : eosio::table<contract_table> {
    index primary_index{
        abieos::name{"primary"}, [](const auto& var) {
            return std::visit(
                [](const auto& obj) { return eosio::check(eosio::convert_to_key(std::tie(obj.code, obj.table, obj.scope))).value(); }, var);
        }};

    contract_table_kv(eosio::kv_environment environment)
        : eosio::table<contract_table>{std::move(environment)} {
        init(abieos::name{"eosio.kvdisk"}, abieos::name{"system"}, abieos::name{"contract.tab"}, primary_index);
    }
};

struct contract_row_kv : eosio::table<contract_row> {
    index primary_index{
        abieos::name{"primary"}, [](const auto& var) {
            return std::visit(
                [](const auto& obj) {
                    return eosio::check(eosio::convert_to_key(std::tie(obj.code, obj.table, obj.scope, obj.primary_key))).value();
                },
                var);
        }};

    contract_row_kv(eosio::kv_environment environment)
        : eosio::table<contract_row>{std::move(environment)} {
        init(abieos::name{"eosio.kvdisk"}, abieos::name{"system"}, abieos::name{"contract.row"}, primary_index);
    }
};

template <typename Table, typename F>
void store_delta_typed(eosio::kv_environment environment, table_delta_v0& delta, bool bypass_preexist_check, F f) {
    Table table{environment};
    for (auto& row : delta.rows) {
        f();
        auto obj = eosio::check(eosio::from_bin<typename Table::value_type>(row.data)).value();
        if (row.present)
            table.insert(obj, bypass_preexist_check);
        else
            table.erase(obj);
    }
}

template <typename F>
inline void store_delta(eosio::kv_environment environment, table_delta_v0& delta, bool bypass_preexist_check, F f) {
    if (delta.name == "contract_table")
        store_delta_typed<contract_table_kv>(environment, delta, bypass_preexist_check, f);
    if (delta.name == "contract_row")
        store_delta_typed<contract_row_kv>(environment, delta, bypass_preexist_check, f);
}

inline void store_deltas(eosio::kv_environment environment, std::vector<table_delta>& deltas, bool bypass_preexist_check) {
    for (auto& delta : deltas)
        store_delta(environment, std::get<table_delta_v0>(delta), bypass_preexist_check, [] {});
}

} // namespace state_history
