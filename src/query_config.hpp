// copyright defined in LICENSE.txt

#pragma once

#include "abieos_exception.hpp"

namespace query_config {

struct field {
    std::string name       = {};
    std::string short_name = {};
    std::string type       = {};
};

template <typename F>
constexpr void for_each_field(field*, F f) {
    f("name", abieos::member_ptr<&field::name>{});
    f("short_name", abieos::member_ptr<&field::short_name>{});
    f("type", abieos::member_ptr<&field::type>{});
};

struct key {
    std::string name           = {};
    std::string new_name       = {};
    std::string type           = {};
    std::string expression     = {};
    std::string arg_expression = {};
    bool        desc           = {};
};

template <typename F>
constexpr void for_each_field(key*, F f) {
    f("name", abieos::member_ptr<&key::name>{});
    f("new_name", abieos::member_ptr<&key::new_name>{});
    f("type", abieos::member_ptr<&key::type>{});
    f("expression", abieos::member_ptr<&key::expression>{});
    f("arg_expression", abieos::member_ptr<&key::arg_expression>{});
    f("desc", abieos::member_ptr<&key::desc>{});
};

template <typename T>
struct table {
    std::string        name         = {};
    std::vector<field> fields       = {};
    std::vector<T>     types        = {};
    std::vector<key>   history_keys = {};
    std::vector<key>   keys         = {};

    std::map<std::string, field*> field_map = {};
};

template <typename T, typename F>
constexpr void for_each_field(table<T>*, F f) {
    f("name", abieos::member_ptr<&table<T>::name>{});
    f("fields", abieos::member_ptr<&table<T>::fields>{});
    f("history_keys", abieos::member_ptr<&table<T>::history_keys>{});
    f("keys", abieos::member_ptr<&table<T>::keys>{});
};

template <typename T>
struct query {
    abieos::name             wasm_name         = {};
    std::string              index             = {};
    std::string              function          = {};
    std::string              _table            = {};
    bool                     is_state          = {};
    bool                     limit_block_index = {};
    uint32_t                 max_results       = {};
    std::string              join              = {};
    std::vector<key>         args              = {};
    std::vector<key>         sort_keys         = {};
    std::vector<key>         join_key_values   = {};
    std::vector<key>         fields_from_join  = {};
    std::vector<std::string> conditions        = {};

    std::vector<T> arg_types    = {};
    std::vector<T> range_types  = {};
    std::vector<T> result_types = {};
    table<T>*      result_table = {};
    table<T>*      join_table   = {};
};

template <typename T, typename F>
constexpr void for_each_field(query<T>*, F f) {
    f("wasm_name", abieos::member_ptr<&query<T>::wasm_name>{});
    f("index", abieos::member_ptr<&query<T>::index>{});
    f("function", abieos::member_ptr<&query<T>::function>{});
    f("table", abieos::member_ptr<&query<T>::_table>{});
    f("is_state", abieos::member_ptr<&query<T>::is_state>{});
    f("limit_block_index", abieos::member_ptr<&query<T>::limit_block_index>{});
    f("max_results", abieos::member_ptr<&query<T>::max_results>{});
    f("join", abieos::member_ptr<&query<T>::join>{});
    f("args", abieos::member_ptr<&query<T>::args>{});
    f("sort_keys", abieos::member_ptr<&query<T>::sort_keys>{});
    f("join_key_values", abieos::member_ptr<&query<T>::join_key_values>{});
    f("fields_from_join", abieos::member_ptr<&query<T>::fields_from_join>{});
    f("conditions", abieos::member_ptr<&query<T>::conditions>{});
};

template <typename T>
struct config {
    std::vector<table<T>> tables  = {};
    std::vector<query<T>> queries = {};

    std::map<std::string, table<T>*>  table_map = {};
    std::map<abieos::name, query<T>*> query_map = {};

    template <typename M>
    void prepare(const M& type_map) {
        for (auto& table : tables) {
            table_map[table.name] = &table;
            for (auto& field : table.fields) {
                table.field_map[field.name] = &field;
                auto it                     = type_map.find(field.type);
                if (it == type_map.end())
                    throw std::runtime_error("table " + table.name + " field " + field.name + ": unknown type: " + field.type);
                table.types.push_back(it->second);
            }
        }

        for (auto& query : queries) {
            query_map[query.wasm_name] = &query;
            auto it                    = table_map.find(query._table);
            if (it == table_map.end())
                throw std::runtime_error("query " + (std::string)query.wasm_name + ": unknown table: " + query._table);
            query.result_table = it->second;
            for (auto& arg : query.args) {
                auto type_it = type_map.find(arg.type);
                if (type_it == type_map.end())
                    throw std::runtime_error("query " + (std::string)query.wasm_name + " arg " + arg.name + ": unknown type: " + arg.type);
                query.arg_types.push_back(type_it->second);
            }
            auto add_types = [&](auto& dest, auto& fields, table<T>* t) {
                for (auto& key : fields) {
                    std::string type = key.type;
                    if (type.empty()) {
                        auto field_it = t->field_map.find(key.name);
                        if (field_it == t->field_map.end())
                            throw std::runtime_error("query " + (std::string)query.wasm_name + ": unknown field: " + key.name);
                        type = field_it->second->type;
                    }

                    auto type_it = type_map.find(type);
                    if (type_it == type_map.end())
                        throw std::runtime_error("query " + (std::string)query.wasm_name + " key " + key.name + ": unknown type: " + type);
                    dest.push_back(type_it->second);
                }
            };
            add_types(query.range_types, query.sort_keys, query.result_table);

            query.result_types = query.result_table->types;
            if (!query.join.empty()) {
                auto it = table_map.find(query.join);
                if (it == table_map.end())
                    throw std::runtime_error("query " + (std::string)query.wasm_name + ": unknown table: " + query.join);
                query.join_table = it->second;
                add_types(query.result_types, query.fields_from_join, query.join_table);
            }
        }
    }
};

template <typename T, typename F>
constexpr void for_each_field(config<T>*, F f) {
    f("tables", abieos::member_ptr<&config<T>::tables>{});
    f("queries", abieos::member_ptr<&config<T>::queries>{});
};

} // namespace query_config
