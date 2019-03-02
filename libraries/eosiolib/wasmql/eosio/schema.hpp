// copyright defined in LICENSE.txt

#pragma once

#include <eosio/to-json.hpp>
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
__attribute__((noinline)) void make_json_schema_recurse(T*, std::vector<char>& dest);

/// \exclude
template <typename T>
void make_json_schema_definitions(T*, std::vector<std::string_view>& existing, std::vector<char>& dest);

// todo: remove
/// \exclude
__attribute__((noinline)) inline void append_str(std::string_view sv, std::vector<char>& dest) {
    dest.insert(dest.end(), sv.begin(), sv.end());
}

/// \exclude
__attribute__((noinline)) inline void make_json_schema_string_pattern(std::string_view pattern, std::vector<char>& dest) {
    using namespace std::literals;
    kv_to_json("type"sv, "string"sv, dest);
    dest.push_back(',');
    kv_to_json("pattern"sv, pattern, dest);
}

/// \group make_json_schema_explicit Make JSON Schema (Explicit Types)
/// Convert types to JSON Schema. Appends to `dest`. The first argument is ignored; it may be `nullptr`. These overloads handle specified types.
__attribute__((noinline)) inline void make_json_schema(std::string_view*, std::vector<char>& dest) {
    using namespace std::literals;
    kv_to_json("type"sv, "string"sv, dest);
}

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(name*) { return "eosio::name"; }

/// \group make_json_schema_explicit
__attribute__((noinline)) inline void make_json_schema(name*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: name pattern ...", dest);
}

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(symbol_code*) { return "eosio::symbol_code"; }

/// \group make_json_schema_explicit
__attribute__((noinline)) inline void make_json_schema(symbol_code*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: symbol_code pattern ...", dest);
}

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(time_point*) { return "eosio::time_point"; }

/// \group make_json_schema_explicit
__attribute__((noinline)) inline void make_json_schema(time_point*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: time_point pattern ...", dest);
}

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(block_timestamp*) { return "eosio::block_timestamp"; }

/// \group make_json_schema_explicit
__attribute__((noinline)) inline void make_json_schema(block_timestamp*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: block_timestamp pattern ...", dest);
}

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(extended_asset*) { return "eosio::extended_asset"; }

/// \group make_json_schema_explicit
__attribute__((noinline)) inline void make_json_schema(extended_asset*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: extended_asset pattern ...", dest);
}

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(serial_wrapper<checksum256>*) { return "eosio::checksum256"; }

/// \group make_json_schema_explicit
__attribute__((noinline)) inline void make_json_schema(serial_wrapper<checksum256>*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: checksum256 pattern ...", dest);
}

/// \group schema_type_name_explicit Get JSON Schema type name (Explicit Types)
inline std::string_view schema_type_name(datastream<const char*>*) { return "eosio::bytes"; }

/// \group make_json_schema_explicit
__attribute__((noinline)) inline void make_json_schema(datastream<const char*>*, std::vector<char>& dest) {
    make_json_schema_string_pattern("... todo: datastream pattern ...", dest);
}

/// \exclude
template <tagged_variant_options Options>
__attribute__((noinline)) inline void make_json_schema_tagged_variant(bool, std::vector<char>& dest) {}

/// \exclude
template <tagged_variant_options Options, typename NamedType, typename... NamedTypes>
__attribute__((noinline)) inline void make_json_schema_tagged_variant(bool need_comma, std::vector<char>& dest);

/// \group make_json_schema_explicit
template <tagged_variant_options Options, typename... NamedTypes>
__attribute__((noinline)) inline void make_json_schema(tagged_variant<Options, NamedTypes...>*, std::vector<char>& dest) {
    using namespace std::literals;
    to_json("oneOf"sv, dest);
    append_str(":[", dest);
    make_json_schema_tagged_variant<Options, NamedTypes...>(false, dest);
    append_str("]", dest);
}

/// \group make_json_schema_explicit
__attribute__((noinline)) inline void make_json_schema(bool*, std::vector<char>& dest) {
    using namespace std::literals;
    kv_to_json("type"sv, "boolean"sv, dest);
}

/// \group make_json_schema_explicit
template <typename T>
__attribute__((noinline)) inline void make_json_schema(std::vector<T>*, std::vector<char>& dest) {
    using namespace std::literals;
    kv_to_json("type"sv, "array"sv, dest);
    dest.push_back(',');
    to_json("items"sv, dest);
    dest.push_back(':');
    make_json_schema_recurse((T*)nullptr, dest);
}

/// \exclude
template <typename T>
__attribute__((noinline)) inline void
make_json_schema_definitions_recurse(std::vector<T>*, std::vector<std::string_view>& existing, std::vector<char>& dest) {
    make_json_schema_definitions((T*)nullptr, existing, dest);
}

/// \group make_json_schema_explicit
template <typename T>
__attribute__((noinline)) inline void make_json_schema(std::optional<T>*, std::vector<char>& dest) {
    using namespace std::literals;
    to_json("oneOf"sv, dest);
    append_str(R"(:[{"type":"null"},)", dest);
    make_json_schema_recurse((T*)nullptr, dest);
    append_str("]", dest);
}

/// \exclude
template <typename T>
__attribute__((noinline)) inline void
make_json_schema_definitions_recurse(std::optional<T>*, std::vector<std::string_view>& existing, std::vector<char>& dest) {
    make_json_schema_definitions((T*)nullptr, existing, dest);
}

/// \output_section Make JSON Schema (Reflected Objects)
/// Convert types to JSON Schema. Appends to `dest`. The first argument is ignored; it may be `nullptr`.
/// This overload works with [reflected objects](standardese://reflection/).
template <typename T>
__attribute__((noinline)) inline void make_json_schema(T*, std::vector<char>& dest) {
    using namespace std::literals;
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

/// \exclude
template <typename T>
struct has_for_each_member {
  private:
    struct F {
        template <typename A, typename B>
        void operator()(const A&, const B&);
    };

    template <typename C>
    static char test(decltype(for_each_member((C*)nullptr, F{}))*);

    template <typename C>
    static long test(...);

  public:
    static constexpr bool value = sizeof(test<T>((void*)nullptr)) == sizeof(char);
};

/// \exclude
template <typename T>
__attribute__((noinline)) inline void
make_json_schema_definitions_recurse(T*, std::vector<std::string_view>& existing, std::vector<char>& dest) {
    if constexpr (has_for_each_member<T>::value)
        for_each_member((T*)nullptr, [&](std::string_view member_name, auto member) { //
            make_json_schema_definitions(member.null, existing, dest);
        });
}

/// \exclude
inline void append_escaped_name(name n, std::vector<char>& dest) {
    char buffer[13];
    auto end = n.write_as_string(buffer, buffer + sizeof(buffer));
    for (auto x = buffer; x != end; ++x) {
        if (*x == '.')
            append_str("\\\\", dest);
        dest.push_back(*x);
    }
}

/// \exclude
template <tagged_variant_options Options, typename NamedType, typename... NamedTypes>
__attribute__((noinline)) inline void
make_json_schema_definitions_recurse_tagged_variant(std::vector<std::string_view>& existing, std::vector<char>& dest);

/// \exclude
template <tagged_variant_options Options>
__attribute__((noinline)) inline void
make_json_schema_definitions_recurse_tagged_variant(std::vector<std::string_view>& existing, std::vector<char>& dest) {}

/// \exclude
template <tagged_variant_options Options, typename NamedType, typename... NamedTypes>
__attribute__((noinline)) inline void
make_json_schema_definitions_recurse_tagged_variant(std::vector<std::string_view>& existing, std::vector<char>& dest) {
    make_json_schema_definitions((typename NamedType::type*)nullptr, existing, dest);
    make_json_schema_definitions_recurse_tagged_variant<Options, NamedTypes...>(existing, dest);
}

/// \exclude
template <tagged_variant_options Options, typename... NamedTypes>
__attribute__((noinline)) inline void make_json_schema_definitions_recurse(
    tagged_variant<Options, NamedTypes...>*, std::vector<std::string_view>& existing, std::vector<char>& dest) {
    make_json_schema_definitions_recurse_tagged_variant<Options, NamedTypes...>(existing, dest);
}

/// \exclude
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

/// \exclude
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

/// \output_section Make JSON Schema (Use This)
/// Convert types to JSON Schema and return result. This overload creates a schema including
/// the `definitions` section. The other overloads assume the definitions already exist.
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

/// \exclude
template <tagged_variant_options Options, typename NamedType, typename... NamedTypes>
__attribute__((noinline)) inline void make_json_schema_tagged_variant(bool need_comma, std::vector<char>& dest) {
    using namespace std::literals;
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

} // namespace eosio
