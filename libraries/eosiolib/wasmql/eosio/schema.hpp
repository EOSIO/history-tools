// copyright defined in LICENSE.txt

#pragma once

#include <eosio/to_json.hpp>
#include <type_traits>

namespace eosio {

/// \output_section Get JSON Schema type name (Default)
/// Get JSON Schema type name. The first argument is ignored; it may be `nullptr`.
/// Returns "", which prevents the type from being in the `definitions` section
/// of the schema.
template <typename T>
std::string_view schema_type_name(T*) {
    return {};
}

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
/// Get JSON Schema type name. The first argument is ignored; it may be `nullptr`.
inline std::string_view schema_type_name(uint8_t*) { return "eosio::uint8_t"; }

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(int8_t*) { return "eosio::int8_t"; }

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(uint16_t*) { return "eosio::uint16_t"; }

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(int16_t*) { return "eosio::int16_t"; }

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(uint32_t*) { return "eosio::uint32_t"; }

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(int32_t*) { return "eosio::int32_t"; }

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(uint64_t*) { return "eosio::uint64_t"; }

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(int64_t*) { return "eosio::int64_t"; }

/// \exclude
template <typename T>
__attribute__((noinline)) rope make_json_schema_recurse(T*);

/// \exclude
template <typename T>
rope make_json_schema_definitions(T*, std::vector<std::string_view>& existing);

/// \exclude
__attribute__((noinline)) inline rope make_json_schema_string_pattern(std::string_view pattern) {
    return rope{"\"type\":\"string\",\"pattern\":\""} + pattern + "\"";
}

/// \group make_json_schema_explicit Make JSON Schema (Explicit Types)
/// Convert types to JSON Schema. Appends to `dest`. The first argument is ignored; it may be `nullptr`. These overloads handle specified types.
__attribute__((noinline)) inline rope make_json_schema(std::string_view*) { return "\"type\":\"string\""; }

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(name*) { return "eosio::name"; }

/// \group make_json_schema_explicit
__attribute__((noinline)) inline rope make_json_schema(name*) { return make_json_schema_string_pattern("... todo: name pattern ..."); }

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(symbol_code*) { return "eosio::symbol_code"; }

/// \group make_json_schema_explicit
__attribute__((noinline)) inline rope make_json_schema(symbol_code*) {
    return make_json_schema_string_pattern("... todo: symbol_code pattern ...");
}

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(time_point*) { return "eosio::time_point"; }

/// \group make_json_schema_explicit
__attribute__((noinline)) inline rope make_json_schema(time_point*) {
    return make_json_schema_string_pattern("... todo: time_point pattern ...");
}

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(block_timestamp*) { return "eosio::block_timestamp"; }

/// \group make_json_schema_explicit
__attribute__((noinline)) inline rope make_json_schema(block_timestamp*) {
    return make_json_schema_string_pattern("... todo: block_timestamp pattern ...");
}

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(extended_asset*) { return "eosio::extended_asset"; }

/// \group make_json_schema_explicit
__attribute__((noinline)) inline rope make_json_schema(extended_asset*) {
    return make_json_schema_string_pattern("... todo: extended_asset pattern ...");
}

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(checksum256*) { return "eosio::checksum256"; }

/// \group make_json_schema_explicit
__attribute__((noinline)) inline rope make_json_schema(checksum256*) {
    return make_json_schema_string_pattern("... todo: checksum256 pattern ...");
}

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(datastream<const char*>*) { return "eosio::bytes"; }

/// \group make_json_schema_explicit
__attribute__((noinline)) inline rope make_json_schema(datastream<const char*>*) {
    return make_json_schema_string_pattern("... todo: datastream pattern ...");
}

/// \exclude
template <tagged_variant_options Options>
__attribute__((noinline)) inline rope make_json_schema_tagged_variant(bool) {
    return "";
}

/// \exclude
template <tagged_variant_options Options, typename NamedType, typename... NamedTypes>
__attribute__((noinline)) inline rope make_json_schema_tagged_variant(bool need_comma);

/// \group make_json_schema_explicit
template <tagged_variant_options Options, typename... NamedTypes>
__attribute__((noinline)) inline rope make_json_schema(tagged_variant<Options, NamedTypes...>*) {
    return rope{"\"oneOf\":["} + make_json_schema_tagged_variant<Options, NamedTypes...>(false) + "]";
}

/// \group make_json_schema_explicit
__attribute__((noinline)) inline rope make_json_schema(bool*) { return "\"type\":\"boolean\""; }

/// \group make_json_schema_explicit
template <typename T>
__attribute__((noinline)) inline rope make_json_schema(std::vector<T>*) {
    return rope{"\"type\":\"array\",\"items\":"} + make_json_schema_recurse((T*)nullptr);
}

/// \exclude
template <typename T>
__attribute__((noinline)) inline rope make_json_schema_definitions_recurse(std::vector<T>*, std::vector<std::string_view>& existing) {
    return make_json_schema_definitions((T*)nullptr, existing);
}

/// \group make_json_schema_explicit
template <typename T>
__attribute__((noinline)) inline rope make_json_schema(std::optional<T>*) {
    return rope{R"("oneOf":[{"type":"null"},)"} + make_json_schema_recurse((T*)nullptr) + "]";
}

/// \exclude
template <typename T>
__attribute__((noinline)) inline rope make_json_schema_definitions_recurse(std::optional<T>*, std::vector<std::string_view>& existing) {
    return make_json_schema_definitions((T*)nullptr, existing);
}

/// \output_section Make JSON Schema (Reflected Objects)
/// Convert types to JSON Schema. Appends to `dest`. The first argument is ignored; it may be `nullptr`.
/// This overload works with [reflected objects](standardese://reflection/).
template <typename T>
__attribute__((noinline)) inline rope make_json_schema(T*) {
    using namespace std::literals;
    if constexpr (std::is_integral_v<T>) {
        // todo: range, pattern for string, etc.
        return R"("type":["integer","string"])";
    } else {
        rope result = R"("type":"object","properties":{)";
        bool first  = true;
        for_each_member((T*)nullptr, [&](std::string_view member_name, auto member) {
            if (!first)
                result += ",";
            first = false;
            result += to_json(member_name);
            result += ":";
            result += make_json_schema_recurse(member.null);
        });
        result += "}";
        return result;
    }
}

/// \exclude
template <typename T>
__attribute__((noinline)) inline rope make_json_schema_definitions_recurse(T*, std::vector<std::string_view>& existing) {
    rope result;
    if constexpr (has_for_each_member<T>::value)
        for_each_member((T*)nullptr, [&](std::string_view member_name, auto member) { //
            result += make_json_schema_definitions(member.null, existing);
        });
    return result;
}

/// \exclude
inline rope escape_dots_in_name(name n) {
    using namespace internal_use_do_not_use;
    char        raw[13];
    auto        raw_end = n.write_as_string(raw, raw + sizeof(raw));
    rope_buffer b{13 * 3};
    for (auto x = raw; x != raw_end; ++x) {
        if (*x == '.') {
            *b.pos++ = '\\';
            *b.pos++ = '\\';
        }
        *b.pos++ += *x;
    }
    return b;
}

/// \exclude
template <tagged_variant_options Options, typename NamedType, typename... NamedTypes>
__attribute__((noinline)) inline rope make_json_schema_definitions_recurse_tagged_variant(std::vector<std::string_view>& existing);

/// \exclude
template <tagged_variant_options Options>
__attribute__((noinline)) inline rope make_json_schema_definitions_recurse_tagged_variant(std::vector<std::string_view>& existing) {
    return {};
}

/// \exclude
template <tagged_variant_options Options, typename NamedType, typename... NamedTypes>
__attribute__((noinline)) inline rope make_json_schema_definitions_recurse_tagged_variant(std::vector<std::string_view>& existing) {
    return make_json_schema_definitions((typename NamedType::type*)nullptr, existing) +
           make_json_schema_definitions_recurse_tagged_variant<Options, NamedTypes...>(existing);
}

/// \exclude
template <tagged_variant_options Options, typename... NamedTypes>
__attribute__((noinline)) inline rope
make_json_schema_definitions_recurse(tagged_variant<Options, NamedTypes...>*, std::vector<std::string_view>& existing) {
    return make_json_schema_definitions_recurse_tagged_variant<Options, NamedTypes...>(existing);
}

/// \exclude
template <typename T>
__attribute__((noinline)) inline rope make_json_schema_recurse(T*) {
    auto name = schema_type_name((T*)nullptr);
    if (!name.empty()) {
        return rope{R"({"$ref":"#/definitions/)"} + name + R"("})";
    } else {
        return rope("{") + make_json_schema((T*)nullptr) + "}";
    }
}

/// \exclude
template <typename T>
__attribute__((noinline)) inline rope make_json_schema_definitions(T*, std::vector<std::string_view>& existing) {
    auto name = schema_type_name((T*)nullptr);
    rope result;
    if (!name.empty()) {
        if (std::find(existing.begin(), existing.end(), name) != existing.end())
            return result;
        if (!existing.empty())
            result += ",";
        existing.push_back(name);
        result += rope{R"(")"} + name + R"(":{)" + make_json_schema((T*)nullptr) + "}";
    }
    result += make_json_schema_definitions_recurse((T*)nullptr, existing);
    return result;
}

/// \output_section Make JSON Schema (Use This)
/// Convert types to JSON Schema and return result. This overload creates a schema including
/// the `definitions` section. The other overloads assume the definitions already exist.
template <typename T>
__attribute__((noinline)) inline rope make_json_schema() {
    std::vector<std::string_view> existing;
    return rope{R"({"definitions":{)"} +                         //
           make_json_schema_definitions((T*)nullptr, existing) + //
           R"(},)" +                                             //
           make_json_schema((T*)nullptr) +                       //
           "}";
}

/// \exclude
template <tagged_variant_options Options, typename NamedType, typename... NamedTypes>
__attribute__((noinline)) inline rope make_json_schema_tagged_variant(bool need_comma) {
    using namespace std::literals;
    rope result;
    if (need_comma)
        result += ",";
    result += R"({"type":"array","items":[{"type":"string","pattern":")" +              //
              escape_dots_in_name(NamedType::name) +                                    //
              R"("},)" + make_json_schema_recurse((typename NamedType::type*)nullptr) + //
              "]}" +                                                                    //
              make_json_schema_tagged_variant<Options, NamedTypes...>(true);
    return result;
}

} // namespace eosio
