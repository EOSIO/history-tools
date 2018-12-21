// copyright defined in LICENSE.txt

#pragma once
#include "lib-named-variant.hpp"
#include "lib-placeholders.hpp"

inline void parse_json_skip_space(char*& pos, char* end) {
    while (pos != end && (*pos == 0x09 || *pos == 0x0a || *pos == 0x0d || *pos == 0x20))
        ++pos;
}

// todo
inline void parse_json_skip_value(char*& pos, char* end) {
    while (pos != end && *pos != ',' && *pos != '}')
        ++pos;
}

inline void parse_json_expect(char*& pos, char* end, char ch, const char* msg) {
    eosio_assert(pos != end && *pos == ch, msg);
    ++pos;
    parse_json_skip_space(pos, end);
}

inline void parse_json_expect_end(char*& pos, char* end) { eosio_assert(pos == end, "expected end of json"); }

template <typename T>
inline void parse_json(T& obj, char*& pos, char* end);

// todo: escapes
inline void parse_json(std::string_view& result, char*& pos, char* end) {
    eosio_assert(pos != end && *pos++ == '"', "expected string");
    auto begin = pos;
    while (pos != end && *pos != '"')
        ++pos;
    auto e = pos;
    eosio_assert(pos != end && *pos++ == '"', "expected end of string");
    result = std::string_view(begin, e - begin);
}

inline void parse_json(uint32_t& result, char*& pos, char* end) {
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
    eosio_assert(found, "expected positive integer");
    parse_json_skip_space(pos, end);
    if (in_str) {
        parse_json_expect(pos, end, '"', "expected positive integer");
        parse_json_skip_space(pos, end);
    }
}

inline void parse_json(bool& result, char*& pos, char* end) {
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
    eosio_assert(false, "expected boolean");
}

inline void parse_json(eosio::name& result, char*& pos, char* end) {
    std::string_view sv;
    parse_json(sv, pos, end);
    result = eosio::name{sv};
}

inline void parse_json(eosio::symbol_code& result, char*& pos, char* end) {
    std::string_view sv;
    parse_json(sv, pos, end);
    result = eosio::symbol_code{sv};
}

inline void parse_json(serial_wrapper<eosio::checksum256>& result, char*& pos, char* end) {
    auto             bytes = reinterpret_cast<char*>(result.value.data());
    std::string_view sv;
    parse_json(sv, pos, end);
    eosio_assert(sv.size() == 64, "expected checksum256");
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
            eosio_assert(false, "expected checksum256");
            return 0;
        };
        auto h   = get_digit();
        auto l   = get_digit();
        bytes[i] = (h << 4) | l;
    }
}

template <typename F>
inline void parse_object(char*& pos, char* end, F f) {
    parse_json_expect(pos, end, '{', "expected {");
    while (true) {
        std::string_view key;
        parse_json(key, pos, end);
        parse_json_expect(pos, end, ':', "expected :");
        f(key);
        if (pos != end && *pos == ',')
            ++pos;
        else
            break;
    }
    parse_json_expect(pos, end, '}', "expected }");
}

template <typename T>
inline void parse_json(T& obj, char*& pos, char* end) {
    parse_object(pos, end, [&](std::string_view key) {
        bool found = false;
        for_each_member(obj, [&](std::string_view member_name, auto& member) {
            if (key == member_name) {
                parse_json(member, pos, end);
                found = true;
            }
        });
        if (!found)
            parse_json_skip_value(pos, end);
    });
}

template <typename T>
inline T parse_json(std::vector<char>&& v) {
    char* pos = v.data();
    char* end = pos + v.size();
    parse_json_skip_space(pos, end);
    T result;
    parse_json(result, pos, end);
    parse_json_expect_end(pos, end);
    return result;
}

template <size_t I, typename... NamedTypes>
void parse_named_variant_impl(named_variant<NamedTypes...>& v, size_t i, char*& pos, char* end) {
    if constexpr (I < sizeof...(NamedTypes)) {
        if (i == I) {
            auto& q = v.value;
            auto& x = q.template emplace<I>();
            parse_json(x, pos, end);
        } else {
            return parse_named_variant_impl<I + 1>(v, i, pos, end);
        }
    } else {
        eosio_assert(false, "invalid variant index");
    }
}

template <typename... NamedTypes>
void parse_json(named_variant<NamedTypes...>& result, char*& pos, char* end) {
    parse_json_skip_space(pos, end);
    parse_json_expect(pos, end, '[', "expected array");

    eosio::name name;
    parse_json(name, pos, end);
    parse_json_expect(pos, end, ',', "expected ,");

    for (size_t i = 0; i < sizeof...(NamedTypes); ++i) {
        if (name == named_variant<NamedTypes...>::keys[i]) {
            parse_named_variant_impl<0>(result, i, pos, end);
            parse_json_expect(pos, end, ']', "expected ]");
            return;
        }
    }
    eosio_assert(false, "invalid variant index name");
}
