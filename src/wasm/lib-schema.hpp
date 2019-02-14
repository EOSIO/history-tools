// copyright defined in LICENSE.txt

#pragma once

#include "lib-to-json.hpp"

#include <type_traits>

// todo: remove
using namespace std::literals;

template <typename T>
std::string_view schema_type_name(T*) {
    return {};
}

inline std::string_view schema_type_name(uint8_t*) { return "eosio::uint8_t"; }
inline std::string_view schema_type_name(int8_t*) { return "eosio::int8_t"; }
inline std::string_view schema_type_name(uint16_t*) { return "eosio::uint16_t"; }
inline std::string_view schema_type_name(int16_t*) { return "eosio::int16_t"; }
inline std::string_view schema_type_name(uint32_t*) { return "eosio::uint32_t"; }
inline std::string_view schema_type_name(int32_t*) { return "eosio::int32_t"; }
inline std::string_view schema_type_name(uint64_t*) { return "eosio::uint64_t"; }
inline std::string_view schema_type_name(int64_t*) { return "eosio::int64_t"; }

template <typename T>
__attribute__((noinline)) void make_json_schema_recurse(T*, std::vector<char>& dest);

template <typename T>
void make_json_schema_definitions(T*, std::vector<std::string_view>& existing, std::vector<char>& dest);

// todo: remove
__attribute__((noinline)) inline void append_str(std::string_view sv, std::vector<char>& dest) {
    dest.insert(dest.end(), sv.begin(), sv.end());
}

__attribute__((noinline)) inline void make_json_schema_string_pattern(std::string_view pattern, std::vector<char>& dest) {
    kv_to_json("type"sv, "string"sv, dest);
    dest.push_back(',');
    kv_to_json("pattern"sv, pattern, dest);
}

__attribute__((noinline)) inline void make_json_schema(std::string_view*, std::vector<char>& dest) {
    kv_to_json("type"sv, "string"sv, dest);
}

// todo: remove these
__attribute__((noinline)) inline void
make_json_schema_definitions_recurse(std::string_view*, std::vector<std::string_view>& existing, std::vector<char>& dest) {}

inline std::string_view schema_type_name(eosio::name*) { return "eosio::name"; }

__attribute__((noinline)) inline void
make_json_schema_definitions_recurse(eosio::name*, std::vector<std::string_view>& existing, std::vector<char>& dest) {}

__attribute__((noinline)) inline void make_json_schema(eosio::name*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: name pattern ...", dest);
}

inline std::string_view schema_type_name(eosio::symbol_code*) { return "eosio::symbol_code"; }

__attribute__((noinline)) inline void
make_json_schema_definitions_recurse(eosio::symbol_code*, std::vector<std::string_view>& existing, std::vector<char>& dest) {}

__attribute__((noinline)) inline void make_json_schema(eosio::symbol_code*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: symbol_code pattern ...", dest);
}

inline std::string_view schema_type_name(eosio::time_point*) { return "eosio::time_point"; }

__attribute__((noinline)) inline void
make_json_schema_definitions_recurse(eosio::time_point*, std::vector<std::string_view>& existing, std::vector<char>& dest) {}

__attribute__((noinline)) inline void make_json_schema(eosio::time_point*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: time_point pattern ...", dest);
}

inline std::string_view schema_type_name(eosio::block_timestamp*) { return "eosio::block_timestamp"; }

__attribute__((noinline)) inline void
make_json_schema_definitions_recurse(eosio::block_timestamp*, std::vector<std::string_view>& existing, std::vector<char>& dest) {}

__attribute__((noinline)) inline void make_json_schema(eosio::block_timestamp*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: block_timestamp pattern ...", dest);
}

inline std::string_view schema_type_name(eosio::extended_asset*) { return "eosio::extended_asset"; }

__attribute__((noinline)) inline void
make_json_schema_definitions_recurse(eosio::extended_asset*, std::vector<std::string_view>& existing, std::vector<char>& dest) {}

__attribute__((noinline)) inline void make_json_schema(eosio::extended_asset*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: extended_asset pattern ...", dest);
}

inline std::string_view schema_type_name(serial_wrapper<eosio::checksum256>*) { return "eosio::checksum256"; }

__attribute__((noinline)) inline void make_json_schema_definitions_recurse(
    serial_wrapper<eosio::checksum256>*, std::vector<std::string_view>& existing, std::vector<char>& dest) {}

__attribute__((noinline)) inline void make_json_schema(serial_wrapper<eosio::checksum256>*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: checksum256 pattern ...", dest);
}

inline std::string_view schema_type_name(eosio::datastream<const char*>*) { return "eosio::bytes"; }

__attribute__((noinline)) inline void
make_json_schema_definitions_recurse(eosio::datastream<const char*>*, std::vector<std::string_view>& existing, std::vector<char>& dest) {}

__attribute__((noinline)) inline void make_json_schema(eosio::datastream<const char*>*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: datastream pattern ...", dest);
}

template <tagged_variant_options Options>
__attribute__((noinline)) inline void make_json_schema_tagged_variant(bool, std::vector<char>& dest) {}

template <tagged_variant_options Options, typename NamedType, typename... NamedTypes>
__attribute__((noinline)) inline void make_json_schema_tagged_variant(bool need_comma, std::vector<char>& dest);

template <tagged_variant_options Options, typename... NamedTypes>
__attribute__((noinline)) inline void make_json_schema(tagged_variant<Options, NamedTypes...>*, std::vector<char>& dest) {
    to_json("oneOf"sv, dest);
    append_str(":[", dest);
    make_json_schema_tagged_variant<Options, NamedTypes...>(false, dest);
    append_str("]", dest);
}

__attribute__((noinline)) inline void make_json_schema(bool*, std::vector<char>& dest) { kv_to_json("type"sv, "boolean"sv, dest); }

template <typename T>
__attribute__((noinline)) inline void make_json_schema(std::vector<T>*, std::vector<char>& dest) {
    kv_to_json("type"sv, "array"sv, dest);
    dest.push_back(',');
    to_json("items"sv, dest);
    dest.push_back(':');
    make_json_schema_recurse((T*)nullptr, dest);
}

template <typename T>
__attribute__((noinline)) inline void
make_json_schema_definitions_recurse(std::vector<T>*, std::vector<std::string_view>& existing, std::vector<char>& dest) {
    make_json_schema_definitions((T*)nullptr, existing, dest);
}

template <typename T>
__attribute__((noinline)) inline void make_json_schema(std::optional<T>*, std::vector<char>& dest) {
    to_json("oneOf"sv, dest);
    append_str(R"(:[{"type":"null"},)", dest);
    make_json_schema_recurse((T*)nullptr, dest);
    append_str("]", dest);
}

template <typename T>
__attribute__((noinline)) inline void
make_json_schema_definitions_recurse(std::optional<T>*, std::vector<std::string_view>& existing, std::vector<char>& dest) {
    make_json_schema_definitions((T*)nullptr, existing, dest);
}

template <typename T>
__attribute__((noinline)) inline void make_json_schema(T*, std::vector<char>& dest) {
    if constexpr (std::is_integral_v<T>) {
        // todo: range, pattern for string, etc.
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
            make_json_schema_recurse(member.null, dest);
        });
        dest.push_back('}');
    }
}

template <typename T>
__attribute__((noinline)) inline void
make_json_schema_definitions_recurse(T*, std::vector<std::string_view>& existing, std::vector<char>& dest) {
    if constexpr (!std::is_integral_v<T>)
        for_each_member((T*)nullptr, [&](std::string_view member_name, auto member) { //
            make_json_schema_definitions(member.null, existing, dest);
        });
}

inline void append_escaped_name(eosio::name n, std::vector<char>& dest) {
    char buffer[13];
    auto end = n.write_as_string(buffer, buffer + sizeof(buffer));
    for (auto x = buffer; x != end; ++x) {
        if (*x == '.')
            append_str("\\\\", dest);
        dest.push_back(*x);
    }
}

template <tagged_variant_options Options, typename NamedType, typename... NamedTypes>
__attribute__((noinline)) inline void make_json_schema_tagged_variant(bool need_comma, std::vector<char>& dest) {
    if (need_comma)
        dest.push_back(',');
    dest.push_back('{');
    kv_to_json("type"sv, "array"sv, dest);
    dest.push_back(',');
    to_json("items"sv, dest);
    append_str(":[{", dest);
    kv_to_json("type"sv, "string"sv, dest);
    append_str(R"(,"pattern":")", dest);
    append_escaped_name(NamedType::name, dest);
    append_str("\"},", dest);
    make_json_schema_recurse((typename NamedType::type*)nullptr, dest);
    append_str("]}", dest);
    make_json_schema_tagged_variant<Options, NamedTypes...>(true, dest);
}

template <tagged_variant_options Options, typename NamedType, typename... NamedTypes>
__attribute__((noinline)) inline void
make_json_schema_definitions_recurse_tagged_variant(std::vector<std::string_view>& existing, std::vector<char>& dest);

template <tagged_variant_options Options>
__attribute__((noinline)) inline void
make_json_schema_definitions_recurse_tagged_variant(std::vector<std::string_view>& existing, std::vector<char>& dest) {}

template <tagged_variant_options Options, typename NamedType, typename... NamedTypes>
__attribute__((noinline)) inline void
make_json_schema_definitions_recurse_tagged_variant(std::vector<std::string_view>& existing, std::vector<char>& dest) {
    make_json_schema_definitions((typename NamedType::type*)nullptr, existing, dest);
    make_json_schema_definitions_recurse_tagged_variant<Options, NamedTypes...>(existing, dest);
}

template <tagged_variant_options Options, typename... NamedTypes>
__attribute__((noinline)) inline void make_json_schema_definitions_recurse(
    tagged_variant<Options, NamedTypes...>*, std::vector<std::string_view>& existing, std::vector<char>& dest) {
    make_json_schema_definitions_recurse_tagged_variant<Options, NamedTypes...>(existing, dest);
}

template <typename T>
__attribute__((noinline)) inline void make_json_schema_recurse(T*, std::vector<char>& dest) {
    auto name = schema_type_name((T*)nullptr);
    if (!name.empty()) {
        append_str(R"({"$ref":"#/definitions/)", dest);
        dest.insert(dest.end(), name.begin(), name.end());
        append_str(R"("})", dest);
    } else {
        dest.push_back('{');
        make_json_schema((T*)nullptr, dest);
        dest.push_back('}');
    }
}

template <typename T>
__attribute__((noinline)) inline void make_json_schema_definitions(T*, std::vector<std::string_view>& existing, std::vector<char>& dest) {
    auto name = schema_type_name((T*)nullptr);
    if (!name.empty()) {
        if (std::find(existing.begin(), existing.end(), name) != existing.end())
            return;
        if (!existing.empty())
            dest.push_back(',');
        existing.push_back(name);
        append_str(R"(")", dest);
        dest.insert(dest.end(), name.begin(), name.end());
        append_str(R"(":{)", dest);
        make_json_schema((T*)nullptr, dest);
        dest.push_back('}');
    }
    make_json_schema_definitions_recurse((T*)nullptr, existing, dest);
}

template <typename T>
__attribute__((noinline)) inline std::vector<char> make_json_schema(T* = nullptr) {
    std::vector<char> result;
    result.push_back('{');
    append_str(R"("definitions":{)", result);
    std::vector<std::string_view> existing;
    make_json_schema_definitions((T*)nullptr, existing, result);
    append_str(R"(},)", result);
    make_json_schema((T*)nullptr, result);
    result.push_back('}');
    return result;
}
