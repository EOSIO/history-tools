// copyright defined in LICENSE.txt

#pragma once
#include <eosio/tagged_variant.hpp>
#include <eosio/temp_placeholders.hpp>

namespace eosio {

/// \exclude
void parse_json_skip_space(const char*& pos, const char* end);

/// \exclude
void parse_json_skip_value(const char*& pos, const char* end);

/// \exclude
void parse_json_expect(const char*& pos, const char* end, char ch, const char* msg);

/// \exclude
void parse_json_expect_end(const char*& pos, const char* end);

/// \exclude
template <typename T>
__attribute__((noinline)) inline void parse_json(T& result, const char*& pos, const char* end);

// todo: escapes
/// \group parse_json_explicit Parse JSON (Explicit Types)
/// Parse JSON and convert to `result`. These overloads handle specified types.
__attribute__((noinline)) inline void parse_json(std::string_view& result, const char*& pos, const char* end) {
    check(pos != end && *pos++ == '"', "expected string");
    auto begin = pos;
    while (pos != end && *pos != '"')
        ++pos;
    auto e = pos;
    check(pos != end && *pos++ == '"', "expected end of string");
    result = std::string_view(begin, e - begin);
}

/// \group parse_json_explicit
__attribute__((noinline)) inline void parse_json(uint32_t& result, const char*& pos, const char* end) {
    bool in_str = false;
    if (pos != end && *pos == '"') {
        in_str = true;
        parse_json_skip_space(pos, end);
    }
    bool found = false;
    result     = 0;
    while (pos != end && *pos >= '0' && *pos <= '9') {
        result = result * 10 + *pos++ - '0';
        found  = true;
    }
    check(found, "expected positive integer");
    parse_json_skip_space(pos, end);
    if (in_str) {
        parse_json_expect(pos, end, '"', "expected positive integer");
        parse_json_skip_space(pos, end);
    }
}

/// \group parse_json_explicit
__attribute__((noinline)) inline void parse_json(int32_t& result, const char*& pos, const char* end) {
    bool in_str = false;
    if (pos != end && *pos == '"') {
        in_str = true;
        parse_json_skip_space(pos, end);
    }
    bool neg = false;
    if (pos != end && *pos == '-') {
        neg = true;
        ++pos;
    }
    bool found = false;
    result     = 0;
    while (pos != end && *pos >= '0' && *pos <= '9') {
        result = result * 10 + *pos++ - '0';
        found  = true;
    }
    check(found, "expected integer");
    parse_json_skip_space(pos, end);
    if (in_str) {
        parse_json_expect(pos, end, '"', "expected integer");
        parse_json_skip_space(pos, end);
    }
    if (neg)
        result = -result;
}

/// \group parse_json_explicit
__attribute__((noinline)) inline void parse_json(bool& result, const char*& pos, const char* end) {
    if (end - pos >= 4 && !strncmp(pos, "true", 4)) {
        pos += 4;
        result = true;
        return parse_json_skip_space(pos, end);
    }
    if (end - pos >= 5 && !strncmp(pos, "false", 5)) {
        pos += 5;
        result = false;
        return parse_json_skip_space(pos, end);
    }
    check(false, "expected boolean");
}

/// \group parse_json_explicit
__attribute__((noinline)) inline void parse_json(name& result, const char*& pos, const char* end) {
    std::string_view sv;
    parse_json(sv, pos, end);
    result = name{sv};
}

/// \group parse_json_explicit
__attribute__((noinline)) inline void parse_json(symbol_code& result, const char*& pos, const char* end) {
    std::string_view sv;
    parse_json(sv, pos, end);
    result = symbol_code{sv};
}

// todo: fix byte order
/// \group parse_json_explicit
__attribute__((noinline)) inline void parse_json(serial_wrapper<checksum256>& result, const char*& pos, const char* end) {
    auto             bytes = reinterpret_cast<char*>(result.value.data());
    std::string_view sv;
    parse_json(sv, pos, end);
    check(sv.size() == 64, "expected checksum256");
    auto p = sv.begin();
    for (int i = 0; i < 32; ++i) {
        auto get_digit = [&]() {
            auto ch = *p++;
            if (ch >= '0' && ch <= '9')
                return ch - '0';
            if (ch >= 'a' && ch <= 'f')
                return ch - 'a' + 10;
            if (ch >= 'A' && ch <= 'F')
                return ch - 'A' + 10;
            check(false, "expected checksum256");
            return 0;
        };
        auto h   = get_digit();
        auto l   = get_digit();
        bytes[i] = (h << 4) | l;
    }
}

/// \group parse_json_explicit
template <typename T>
__attribute__((noinline)) inline void parse_json(std::vector<T>& result, const char*& pos, const char* end) {
    parse_json_expect(pos, end, '[', "expected [");
    if (pos != end && *pos != ']') {
        while (true) {
            result.emplace_back();
            parse_json(result.back(), pos, end);
            if (pos != end && *pos == ',') {
                ++pos;
                parse_json_skip_space(pos, end);
            } else
                break;
        }
    }
    parse_json_expect(pos, end, ']', "expected ]");
}

/// \exclude
template <typename F>
inline void parse_json_object(const char*& pos, const char* end, F f) {
    parse_json_expect(pos, end, '{', "expected {");
    if (pos != end && *pos != '}') {
        while (true) {
            std::string_view key;
            parse_json(key, pos, end);
            parse_json_expect(pos, end, ':', "expected :");
            f(key);
            if (pos != end && *pos == ',') {
                ++pos;
                parse_json_skip_space(pos, end);
            } else
                break;
        }
    }
    parse_json_expect(pos, end, '}', "expected }");
}

/// \output_section Parse JSON (Reflected Objects)
/// Parse JSON and convert to `result`. This overload works with
/// [reflected objects](standardese://reflection/).
template <typename T>
__attribute__((noinline)) inline void parse_json(T& result, const char*& pos, const char* end) {
    parse_json_object(pos, end, [&](std::string_view key) {
        bool found = false;
        for_each_member((T*)nullptr, [&](std::string_view member_name, auto member) {
            if (key == member_name) {
                parse_json(member_from_void(member, &result), pos, end);
                found = true;
            }
        });
        if (!found)
            parse_json_skip_value(pos, end);
    });
}

/// \output_section Convenience Wrappers
/// Parse JSON and return result. This overload wraps the other `to_json` overloads.
template <typename T>
__attribute__((noinline)) inline T parse_json(const std::vector<char>& v) {
    const char* pos = v.data();
    const char* end = pos + v.size();
    parse_json_skip_space(pos, end);
    T result;
    parse_json(result, pos, end);
    parse_json_expect_end(pos, end);
    return result;
}

/// Parse JSON and return result. This overload wraps the other `to_json` overloads.
template <typename T>
__attribute__((noinline)) inline T parse_json(std::string_view s) {
    const char* pos = s.data();
    const char* end = pos + s.size();
    parse_json_skip_space(pos, end);
    T result;
    parse_json(result, pos, end);
    parse_json_expect_end(pos, end);
    return result;
}

/// \exclude
template <size_t I, tagged_variant_options Options, typename... NamedTypes>
__attribute__((noinline)) void
parse_named_variant_impl(tagged_variant<Options, NamedTypes...>& v, size_t i, const char*& pos, const char* end) {
    if constexpr (I < sizeof...(NamedTypes)) {
        if (i == I) {
            auto& q = v.value;
            auto& x = q.template emplace<I>();
            if constexpr (!is_named_empty_type_v<std::decay_t<decltype(x)>>) {
                parse_json_expect(pos, end, ',', "expected ,");
                parse_json(x, pos, end);
            }
        } else {
            return parse_named_variant_impl<I + 1>(v, i, pos, end);
        }
    } else {
        check(false, "invalid variant index");
    }
}

/// \group parse_json_explicit
template <tagged_variant_options Options, typename... NamedTypes>
__attribute__((noinline)) void parse_json(tagged_variant<Options, NamedTypes...>& result, const char*& pos, const char* end) {
    parse_json_skip_space(pos, end);
    parse_json_expect(pos, end, '[', "expected array");

    eosio::name name;
    parse_json(name, pos, end);

    for (size_t i = 0; i < sizeof...(NamedTypes); ++i) {
        if (name == tagged_variant<Options, NamedTypes...>::keys[i]) {
            parse_named_variant_impl<0>(result, i, pos, end);
            parse_json_expect(pos, end, ']', "expected ]");
            return;
        }
    }
    check(false, "invalid variant index name");
}

/// \output_section JSON Conversion Helpers
/// Skip spaces
__attribute__((noinline)) inline void parse_json_skip_space(const char*& pos, const char* end) {
    while (pos != end && (*pos == 0x09 || *pos == 0x0a || *pos == 0x0d || *pos == 0x20))
        ++pos;
}

// todo
/// Skip a JSON value. Caution: only partially implemented; currently mishandles most cases.
__attribute__((noinline)) inline void parse_json_skip_value(const char*& pos, const char* end) {
    while (pos != end && *pos != ',' && *pos != '}')
        ++pos;
}

/// Asserts `ch` is next character. `msg` is the assertion message.
__attribute__((noinline)) inline void parse_json_expect(const char*& pos, const char* end, char ch, const char* msg) {
    check(pos != end && *pos == ch, msg);
    ++pos;
    parse_json_skip_space(pos, end);
}

/// Asserts `pos == end`.
__attribute__((noinline)) inline void parse_json_expect_end(const char*& pos, const char* end) {
    check(pos == end, "expected end of json");
}

} // namespace eosio
