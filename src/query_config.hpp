// copyright defined in LICENSE.txt

#pragma once

#include "abieos_exception.hpp"

namespace query_config {

template <typename Defs>
struct field {
    std::string                name           = {};
    std::string                short_name     = {};
    std::string                type           = {};
    bool                       begin_optional = {};
    bool                       end_optional   = {};
    const typename Defs::type* type_obj       = {};
};

template <typename Defs, typename F>
constexpr void for_each_field(field<Defs>*, F f) {
    ABIEOS_MEMBER(field<Defs>, name);
    ABIEOS_MEMBER(field<Defs>, short_name);
    ABIEOS_MEMBER(field<Defs>, type);
    ABIEOS_MEMBER(field<Defs>, begin_optional);
    ABIEOS_MEMBER(field<Defs>, end_optional);
};

template <typename Defs>
struct key {
    std::string           name           = {};
    std::string           src_name       = {};
    std::string           new_name       = {};
    std::string           type           = {};
    std::string           expression     = {};
    std::string           arg_expression = {};
    bool                  desc           = {};
    typename Defs::field* field          = {};
};

template <typename Defs, typename F>
constexpr void for_each_field(key<Defs>*, F f) {
    ABIEOS_MEMBER(key<Defs>, name);
    ABIEOS_MEMBER(key<Defs>, src_name);
    ABIEOS_MEMBER(key<Defs>, new_name);
    ABIEOS_MEMBER(key<Defs>, type);
    ABIEOS_MEMBER(key<Defs>, expression);
    ABIEOS_MEMBER(key<Defs>, arg_expression);
    ABIEOS_MEMBER(key<Defs>, desc);
};

template <typename Defs>
struct table {
    std::string                                  name         = {};
    std::vector<typename Defs::field>            fields       = {};
    std::vector<typename Defs::key>              history_keys = {};
    std::vector<typename Defs::key>              keys         = {};
    std::map<std::string, typename Defs::field*> field_map    = {};
};

template <typename Defs, typename F>
constexpr void for_each_field(table<Defs>*, F f) {
    ABIEOS_MEMBER(table<Defs>, name);
    ABIEOS_MEMBER(table<Defs>, fields);
    ABIEOS_MEMBER(table<Defs>, history_keys);
    ABIEOS_MEMBER(table<Defs>, keys);
};

template <typename Defs>
struct query {
    abieos::name                      wasm_name            = {};
    std::string                       index                = {};
    std::string                       function             = {};
    std::string                       table                = {};
    bool                              is_state             = {};
    bool                              limit_block_num      = {};
    uint32_t                          max_results          = {};
    std::string                       join                 = {};
    abieos::name                      join_query_wasm_name = {};
    std::vector<typename Defs::key>   args                 = {};
    std::vector<typename Defs::key>   sort_keys            = {};
    std::vector<typename Defs::key>   join_key_values      = {};
    std::vector<typename Defs::key>   fields_from_join     = {};
    std::vector<std::string>          conditions           = {};
    std::vector<typename Defs::type>  arg_types            = {};
    std::vector<typename Defs::type>  range_types          = {};
    std::vector<typename Defs::field> result_fields        = {};
    typename Defs::table*             table_obj            = {};
    typename Defs::table*             join_table           = {};
    query*                            join_query           = {};
};

template <typename Defs, typename F>
constexpr void for_each_field(query<Defs>*, F f) {
    ABIEOS_MEMBER(query<Defs>, wasm_name);
    ABIEOS_MEMBER(query<Defs>, index);
    ABIEOS_MEMBER(query<Defs>, function);
    ABIEOS_MEMBER(query<Defs>, table);
    ABIEOS_MEMBER(query<Defs>, is_state);
    ABIEOS_MEMBER(query<Defs>, limit_block_num);
    ABIEOS_MEMBER(query<Defs>, max_results);
    ABIEOS_MEMBER(query<Defs>, join);
    ABIEOS_MEMBER(query<Defs>, join_query_wasm_name);
    ABIEOS_MEMBER(query<Defs>, args);
    ABIEOS_MEMBER(query<Defs>, sort_keys);
    ABIEOS_MEMBER(query<Defs>, join_key_values);
    ABIEOS_MEMBER(query<Defs>, fields_from_join);
    ABIEOS_MEMBER(query<Defs>, conditions);
};

template <typename Defs, typename Key>
void set_key_fields(const table<Defs>& tab, std::vector<Key>& keys) {
    for (auto& k : keys) {
        auto it = tab.field_map.find(k.name);
        if (it == tab.field_map.end())
            throw std::runtime_error("key references unknown field " + k.name + " in table " + tab.name);
        k.field = it->second;
    }
}

template <typename Defs, typename Key>
void set_join_key_fields(const table<Defs>& tab, std::vector<Key>& keys) {
    for (auto& k : keys) {
        auto it = tab.field_map.find(k.src_name);
        if (it == tab.field_map.end())
            throw std::runtime_error("key references unknown field " + k.src_name + " in table " + tab.name);
        k.field = it->second;
    }
}

template <typename Defs>
struct config {
    std::vector<typename Defs::table>             tables    = {};
    std::vector<typename Defs::query>             queries   = {};
    std::map<std::string, typename Defs::table*>  table_map = {};
    std::map<abieos::name, typename Defs::query*> query_map = {};

    template <typename M>
    void prepare(const M& type_map) {
        for (auto& table : tables) {
            table_map[table.name] = &table;
            for (auto& field : table.fields) {
                table.field_map[field.name] = &field;
                auto it                     = type_map.find(field.type);
                if (it == type_map.end())
                    throw std::runtime_error("table " + table.name + " field " + field.name + ": unknown type: " + field.type);
                field.type_obj = &it->second;
            }
            set_key_fields(table, table.history_keys);
            set_key_fields(table, table.keys);
        }

        for (auto& query : queries) {
            query_map[query.wasm_name] = &query;
            auto it                    = table_map.find(query.table);
            if (it == table_map.end())
                throw std::runtime_error("query " + (std::string)query.wasm_name + ": unknown table: " + query.table);
            query.table_obj = it->second;
            set_key_fields(*query.table_obj, query.args);
            set_key_fields(*query.table_obj, query.sort_keys);
            set_join_key_fields(*query.table_obj, query.join_key_values);
            for (auto& arg : query.args) {
                auto type_it = type_map.find(arg.type);
                if (type_it == type_map.end())
                    throw std::runtime_error("query " + (std::string)query.wasm_name + " arg " + arg.name + ": unknown type: " + arg.type);
                query.arg_types.push_back(type_it->second);
            }
            auto add_types = [&](auto& dest, auto& fields, auto* t) {
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
            add_types(query.range_types, query.sort_keys, query.table_obj);

            query.result_fields = query.table_obj->fields;
            if (!query.join.empty()) {
                auto it = table_map.find(query.join);
                if (it == table_map.end())
                    throw std::runtime_error("query " + (std::string)query.wasm_name + ": unknown table: " + query.join);
                query.join_table = it->second;
                set_key_fields(*query.join_table, query.fields_from_join);
                for (auto& key : query.fields_from_join)
                    query.result_fields.push_back(*key.field);

                auto it2 = query_map.find(query.join_query_wasm_name);
                if (it2 == query_map.end())
                    throw std::runtime_error(
                        "query " + (std::string)query.wasm_name +
                        ": unknown join_query_wasm_name: " + (std::string)query.join_query_wasm_name);
                query.join_query = it2->second;
            }
        }
    } // prepare()
};    // config

template <typename Defs, typename F>
constexpr void for_each_field(config<Defs>*, F f) {
    ABIEOS_MEMBER(config<Defs>, tables);
    ABIEOS_MEMBER(config<Defs>, queries);
};

} // namespace query_config
