#pragma once
#include <eosio/abi.hpp>
#include <string>
#include "state_history_pg.hpp"

struct abieos_sql_converter {

    struct sql_type {
        const char* name                                               = "";
        std::string (*bin_to_sql)(eosio::input_stream&)                = nullptr;
    };

    struct field_def {
        std::string name, type;
    };

    enum field_kind_t {
        table_field,
        composite_field
    };

    using basic_converters_t = std::map<std::string_view, sql_type>;
    struct union_fields_t : std::vector<field_def> {
        union_fields_t(std::string schema_name, const eosio::abi_type::variant& elements, const basic_converters_t&);
    };

    using variant_fields_table = std::map<std::string, union_fields_t>;


    std::string           schema_name;
    std::set<std::string> created_composite_types;
    variant_fields_table  variant_union_fields;
    basic_converters_t    basic_converters;

    template <typename T>
    void register_basic_types() {
        std::apply(
            [this](auto... x) {
                using namespace state_history::pg;
                (basic_converters.try_emplace(names_for<decltype(x)>.abi, sql_type{names_for<decltype(x)>.sql, bin_to_sql<decltype(x)>}),
                 ...);
            },
            T{});
    }

    void        create_sql_type(const eosio::abi_type* type, const std::function<void(std::string)>& exec, bool nested_only);
    std::string create_sql_type(
        std::string name, const eosio::abi_type::struct_* struct_abi_type, const std::function<void(std::string)>& exec, bool nested_only);
    std::string
    create_sql_type(std::string name, const eosio::abi_type::variant* variant_abi_type, const std::function<void(std::string)>& exec);

    void create_table(
        std::string table_name, const eosio::abi_type& type, std::string fields_prefix, const std::vector<std::string>& keys,
        const std::function<void(std::string)>& exec);

    std::string to_sql_value(eosio::input_stream& bin, const eosio::abi_type& type, field_kind_t field_kind = table_field);
    void to_sql_values(eosio::input_stream& bin, const eosio::abi_type::struct_&, std::vector<std::string>& values, field_kind_t field_kind = table_field);
    void to_sql_values(
        eosio::input_stream& bin, std::string type_name, const eosio::abi_type::variant&, std::vector<std::string>& values,
        field_kind_t field_kind = table_field);
};