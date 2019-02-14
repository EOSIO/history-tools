// copyright defined in LICENSE.txt

#pragma once

#include "lib-to-json.hpp"

#include <type_traits>

// todo: remove
using namespace std::literals;

// todo: remove
__attribute__((noinline)) inline void append_str(std::string_view sv, std::vector<char>& dest) {
    dest.insert(dest.end(), sv.begin(), sv.end());
}

__attribute__((noinline)) inline void make_json_schema_string_pattern(std::string_view pattern, std::vector<char>& dest) {
    dest.push_back('{');
    kv_to_json("type"sv, "string"sv, dest);
    dest.push_back(',');
    kv_to_json("pattern"sv, pattern, dest);
    dest.push_back('}');
}

__attribute__((noinline)) inline void make_json_schema(std::string_view*, std::vector<char>& dest) {
    dest.push_back('{');
    kv_to_json("type"sv, "string"sv, dest);
    dest.push_back('}');
}

__attribute__((noinline)) inline void make_json_schema(eosio::name*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: name pattern ...", dest);
}

__attribute__((noinline)) inline void make_json_schema(eosio::symbol_code*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: symbol_code pattern ...", dest);
}

__attribute__((noinline)) inline void make_json_schema(eosio::time_point*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: time_point pattern ...", dest);
}

__attribute__((noinline)) inline void make_json_schema(eosio::block_timestamp*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: block_timestamp pattern ...", dest);
}

__attribute__((noinline)) inline void make_json_schema(eosio::extended_asset*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: extended_asset pattern ...", dest);
}

__attribute__((noinline)) inline void make_json_schema(serial_wrapper<eosio::checksum256>*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: checksum256 pattern ...", dest);
}

__attribute__((noinline)) inline void make_json_schema(eosio::datastream<const char*>*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: datastream pattern ...", dest);
}

template <tagged_variant_options Options>
__attribute__((noinline)) inline void make_json_schema_tagged_variant(bool, std::vector<char>& dest) {}

template <tagged_variant_options Options, typename NamedType, typename... NamedTypes>
__attribute__((noinline)) inline void make_json_schema_tagged_variant(bool needComma, std::vector<char>& dest);

template <tagged_variant_options Options, typename... NamedTypes>
__attribute__((noinline)) inline void make_json_schema(tagged_variant<Options, NamedTypes...>*, std::vector<char>& dest) {
    dest.push_back('{');
    to_json("oneOf"sv, dest);
    append_str(":[", dest);
    make_json_schema_tagged_variant<Options, NamedTypes...>(false, dest);
    append_str("]}", dest);
}

__attribute__((noinline)) inline void make_json_schema(bool*, std::vector<char>& dest) {
    dest.push_back('{');
    kv_to_json("type"sv, "boolean"sv, dest);
    dest.push_back('}');
}

template <typename T>
__attribute__((noinline)) inline void make_json_schema(std::vector<T>*, std::vector<char>& dest) {

    dest.push_back('{');
    kv_to_json("type"sv, "array"sv, dest);
    dest.push_back(',');
    to_json("items"sv, dest);
    dest.push_back(':');
    make_json_schema((T*)nullptr, dest);
    dest.push_back('}');
}

template <typename T>
__attribute__((noinline)) inline void make_json_schema(std::optional<T>*, std::vector<char>& dest) {

    dest.push_back('{');
    to_json("oneOf"sv, dest);
    append_str(R"(:[{"type":"null"},)", dest);
    make_json_schema((T*)nullptr, dest);
    append_str("]}", dest);
}

template <typename T>
__attribute__((noinline)) inline void make_json_schema(T*, std::vector<char>& dest) {
    dest.push_back('{');
    if constexpr (std::is_integral_v<T>) {
        to_json("type"sv, dest);
        append_str(":[", dest);
        to_json("integer"sv, dest);
        dest.push_back(',');
        to_json("string"sv, dest);
        dest.push_back(']');
    } else {
        kv_to_json("type"sv, "object"sv, dest);
        dest.push_back(',');
        to_json("properties"sv, dest);
        append_str(":{", dest);
        bool first = true;
        for_each_member((T*)nullptr, [&](std::string_view member_name, auto member) {
            if (!first)
                dest.push_back(',');
            first = false;
            to_json(member_name, dest);
            dest.push_back(':');
            make_json_schema(member.null, dest);
        });
        dest.push_back('}');
    }
    dest.push_back('}');
}

template <typename T>
__attribute__((noinline)) inline std::vector<char> make_json_schema(T* = nullptr) {
    std::vector<char> result;
    make_json_schema((T*)nullptr, result);
    return result;
}

template <tagged_variant_options Options, typename NamedType, typename... NamedTypes>
__attribute__((noinline)) inline void make_json_schema_tagged_variant(bool needComma, std::vector<char>& dest) {
    if (needComma)
        dest.push_back(',');
    dest.push_back('{');
    kv_to_json("type"sv, "array"sv, dest);
    dest.push_back(',');
    to_json("items"sv, dest);
    append_str(":[{", dest);
    kv_to_json("type"sv, "string"sv, dest);
    append_str(R"(,"pattern":")", dest);
    char buffer[13];
    auto end = NamedType::name.write_as_string(buffer, buffer + sizeof(buffer));
    for (auto x = buffer; x != end; ++x) {
        if (*x == '.')
            append_str("\\\\", dest);
        dest.push_back(*x);
    }
    append_str("\"},", dest);
    make_json_schema((typename NamedType::type*)nullptr, dest);
    append_str("]}", dest);
    make_json_schema_tagged_variant<Options, NamedTypes...>(true, dest);
}
