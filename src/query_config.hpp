// copyright defined in LICENSE.txt

#pragma once

#include <eosio/reflection.hpp>

namespace query_config {

template <typename Defs>
struct field {
    std::string                name           = {};
    std::string                type           = {};
    bool                       begin_optional = {};
    bool                       end_optional   = {};
    const typename Defs::type* type_obj       = {};
};

template <typename Defs, typename F>
constexpr void for_each_field(field<Defs>*, F f) {
    EOSIO_REFLECT_MEMBER(field<Defs>, name);
    EOSIO_REFLECT_MEMBER(field<Defs>, type);
    EOSIO_REFLECT_MEMBER(field<Defs>, begin_optional);
    EOSIO_REFLECT_MEMBER(field<Defs>, end_optional);
};

template <typename Defs>
struct key {
    std::string                 name           = {};
    std::string                 join_src_name  = {};
    std::string                 join_new_name  = {};
    std::string                 expression     = {};
    std::string                 arg_expression = {};
    const typename Defs::field* field          = {};
};

template <typename Defs, typename F>
constexpr void for_each_field(key<Defs>*, F f) {
    EOSIO_REFLECT_MEMBER(key<Defs>, name);
    EOSIO_REFLECT_MEMBER(key<Defs>, join_src_name);
    EOSIO_REFLECT_MEMBER(key<Defs>, join_new_name);
    EOSIO_REFLECT_MEMBER(key<Defs>, expression);
    EOSIO_REFLECT_MEMBER(key<Defs>, arg_expression);
};

template <typename Defs>
struct table {
    std::string                                        name           = {};
    abieos::name                                       short_name     = {};
    std::vector<typename Defs::field>                  fields         = {};
    bool                                               is_delta       = {};
    std::string                                        trim_index     = {};
    std::vector<typename Defs::key>                    keys           = {};
    std::map<std::string, const typename Defs::field*> field_map      = {};
    std::vector<const typename Defs::index*>           indexes        = {};
    std::map<std::string, const typename Defs::index*> index_map      = {};
    const typename Defs::index*                        trim_index_obj = {};
};

template <typename Defs, typename F>
constexpr void for_each_field(table<Defs>*, F f) {
    EOSIO_REFLECT_MEMBER(table<Defs>, name);
    EOSIO_REFLECT_MEMBER(table<Defs>, short_name);
    EOSIO_REFLECT_MEMBER(table<Defs>, fields);
    EOSIO_REFLECT_MEMBER(table<Defs>, is_delta);
    EOSIO_REFLECT_MEMBER(table<Defs>, trim_index);
    EOSIO_REFLECT_MEMBER(table<Defs>, keys);
};

template <typename Defs>
struct index {
    abieos::name                     short_name    = {};
    std::string                      index         = {};
    std::string                      table         = {};
    bool                             include_in_pg = {};
    bool                             only_for_trim = {};
    std::vector<typename Defs::key>  sort_keys     = {};
    std::vector<typename Defs::type> range_types   = {};
    const typename Defs::table*      table_obj     = {};
};

template <typename Defs, typename F>
constexpr void for_each_field(index<Defs>*, F f) {
    EOSIO_REFLECT_MEMBER(index<Defs>, short_name);
    EOSIO_REFLECT_MEMBER(index<Defs>, index);
    EOSIO_REFLECT_MEMBER(index<Defs>, table);
    EOSIO_REFLECT_MEMBER(index<Defs>, include_in_pg);
    EOSIO_REFLECT_MEMBER(index<Defs>, only_for_trim);
    EOSIO_REFLECT_MEMBER(index<Defs>, sort_keys);
};

template <typename Defs>
struct query {
    abieos::name                      short_name            = {};
    std::string                       index                 = {};
    std::string                       function              = {};
    std::string                       table                 = {};
    bool                              has_block_snapshot    = {};
    uint32_t                          max_results           = {};
    std::string                       join                  = {};
    abieos::name                      join_query_short_name = {};
    std::vector<typename Defs::key>   join_key_values       = {};
    std::vector<typename Defs::key>   fields_from_join      = {};
    std::vector<typename Defs::type>  arg_types             = {};
    std::vector<typename Defs::field> result_fields         = {};
    const typename Defs::index*       index_obj             = {};
    const typename Defs::table*       table_obj             = {};
    const typename Defs::table*       join_table            = {};
    const query*                      join_query            = {};
};

template <typename Defs, typename F>
constexpr void for_each_field(query<Defs>*, F f) {
    EOSIO_REFLECT_MEMBER(query<Defs>, short_name);
    EOSIO_REFLECT_MEMBER(query<Defs>, index);
    EOSIO_REFLECT_MEMBER(query<Defs>, function);
    EOSIO_REFLECT_MEMBER(query<Defs>, table);
    EOSIO_REFLECT_MEMBER(query<Defs>, has_block_snapshot);
    EOSIO_REFLECT_MEMBER(query<Defs>, max_results);
    EOSIO_REFLECT_MEMBER(query<Defs>, join);
    EOSIO_REFLECT_MEMBER(query<Defs>, join_query_short_name);
    EOSIO_REFLECT_MEMBER(query<Defs>, join_key_values);
    EOSIO_REFLECT_MEMBER(query<Defs>, fields_from_join);
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
        auto it = tab.field_map.find(k.join_src_name);
        if (it == tab.field_map.end())
            throw std::runtime_error("key references unknown field " + k.join_src_name + " in table " + tab.name);
        k.field = it->second;
    }
}

template <typename Defs>
struct config {
    std::vector<typename Defs::table>                   tables         = {};
    std::vector<typename Defs::index>                   indexes        = {};
    std::vector<typename Defs::query>                   queries        = {};
    std::map<std::string, const typename Defs::table*>  table_map      = {};
    std::map<abieos::name, const typename Defs::table*> table_name_map = {};
    std::map<std::string, const typename Defs::index*>  index_map      = {};
    std::map<abieos::name, const typename Defs::index*> index_name_map = {};
    std::map<abieos::name, const typename Defs::query*> query_map      = {};

    template <typename M>
    void prepare(const M& type_map) {
        for (auto& table : tables) {
            table_map[table.name]            = &table;
            table_name_map[table.short_name] = &table;
            for (auto& field : table.fields) {
                table.field_map[field.name] = &field;
                auto it                     = type_map.find(field.type);
                if (it == type_map.end())
                    throw std::runtime_error("table " + table.name + " field " + field.name + ": unknown type: " + field.type);
                field.type_obj = &it->second;
            }
            set_key_fields(table, table.keys);
        }

        auto add_types = [&](auto& dest, auto& fields, auto* table, auto short_name) {
            for (auto& key : fields) {
                auto field_it = table->field_map.find(key.name);
                if (field_it == table->field_map.end())
                    throw std::runtime_error((std::string)short_name + ": unknown field: " + key.name);
                auto& type    = field_it->second->type;
                auto  type_it = type_map.find(type);
                if (type_it == type_map.end())
                    throw std::runtime_error((std::string)short_name + " key " + key.name + ": unknown type: " + type);
                dest.push_back(type_it->second);
            }
        };

        for (auto& index : indexes) {
            if (index_map.find(index.index) != index_map.end())
                throw std::runtime_error("duplicate index: " + index.index);
            index_map[index.index] = &index;
            if (index_name_map.find(index.short_name) != index_name_map.end())
                throw std::runtime_error("duplicate index: " + (std::string)index.short_name);
            index_name_map[index.short_name] = &index;

            auto it = table_map.find(index.table);
            if (it == table_map.end())
                throw std::runtime_error("index " + (std::string)index.short_name + ": unknown table: " + index.table);
            auto& table     = const_cast<typename Defs::table&>(*it->second);
            index.table_obj = &table;
            table.indexes.push_back(&index);
            if (table.index_map.find(index.index) != table.index_map.end())
                throw std::runtime_error("table '" + index.table + "' has duplicate index '" + index.index + "'");
            table.index_map[index.index] = &index;
            set_key_fields(*index.table_obj, index.sort_keys);
            add_types(index.range_types, index.sort_keys, index.table_obj, index.short_name);
        }

        for (auto& query : queries) {
            query_map[query.short_name] = &query;
            auto index_it               = index_map.find(query.index);
            if (index_it == index_map.end())
                throw std::runtime_error("query " + (std::string)query.short_name + ": unknown index: " + query.index);
            auto it = table_map.find(query.table);
            if (it == table_map.end())
                throw std::runtime_error("query " + (std::string)query.short_name + ": unknown table: " + query.table);

            query.index_obj = index_it->second;
            query.table_obj = it->second;
            if (query.index_obj->only_for_trim)
                throw std::runtime_error(
                    "query '" + (std::string)query.short_name + "': index: '" + query.index + "' is marked only_for_trim");
            set_join_key_fields(*query.table_obj, query.join_key_values);

            query.result_fields = query.table_obj->fields;
            if (!query.join.empty()) {
                auto it = table_map.find(query.join);
                if (it == table_map.end())
                    throw std::runtime_error("query " + (std::string)query.short_name + ": unknown table: " + query.join);
                query.join_table = it->second;
                set_key_fields(*query.join_table, query.fields_from_join);
                for (auto& key : query.fields_from_join)
                    query.result_fields.push_back(*key.field);

                auto it2 = query_map.find(query.join_query_short_name);
                if (it2 == query_map.end())
                    throw std::runtime_error(
                        "query " + (std::string)query.short_name +
                        ": unknown join_query_short_name: " + (std::string)query.join_query_short_name);
                query.join_query = it2->second;
            }
        }

        for (auto& table : tables) {
            if (!table.trim_index.empty()) {
                auto it = table.index_map.find(table.trim_index);
                if (it == table.index_map.end())
                    throw std::runtime_error("table '" + table.name + "' trim_index '" + table.trim_index + "' does not exist");
                table.trim_index_obj = it->second;
            }
        }
    } // prepare()
};    // config

template <typename Defs, typename F>
constexpr void for_each_field(config<Defs>*, F f) {
    EOSIO_REFLECT_MEMBER(config<Defs>, tables);
    EOSIO_REFLECT_MEMBER(config<Defs>, indexes);
    EOSIO_REFLECT_MEMBER(config<Defs>, queries);
};

} // namespace query_config
