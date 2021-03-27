#pragma once
#include <eosio/abi.hpp>
#include <string>

struct abieos_sql_converter {
    struct field_def {
        std::string name, type;
    };

    struct union_fields_t : std::vector<field_def> {
        union_fields_t(std::string schema_name, const eosio::abi_type::variant& elements);
    };

    enum field_kind_t {
        table_field,
        composite_field
    };

    using variant_fields_table = std::map<std::string, union_fields_t>;

    std::string           schema_name;
    std::set<std::string> created_composite_types;
    variant_fields_table  variant_union_fields;

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