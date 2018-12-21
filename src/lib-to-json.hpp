// copyright defined in LICENSE.txt

#pragma once
#include "lib-named-variant.hpp"
#include "lib-placeholders.hpp"
#include <vector>

// todo: replace
inline void append(std::vector<char>& dest, std::string_view sv) { dest.insert(dest.end(), sv.begin(), sv.end()); }

// todo: escape
// todo: handle non-utf8
inline void to_json(std::string_view sv, std::vector<char>& dest) {
    dest.push_back('"');
    dest.insert(dest.end(), sv.begin(), sv.end());
    dest.push_back('"');
}

inline void to_json(uint8_t value, std::vector<char>& dest) {
    char  s[4];
    char* ch = s;
    do {
        *ch++ = '0' + (value % 10);
        value /= 10;
    } while (value);
    std::reverse(s, ch);
    dest.insert(dest.end(), s, ch);
}

inline void to_json(uint32_t value, std::vector<char>& dest) {
    char  s[20];
    char* ch = s;
    do {
        *ch++ = '0' + (value % 10);
        value /= 10;
    } while (value);
    std::reverse(s, ch);
    dest.insert(dest.end(), s, ch);
}

inline void to_json(int64_t value, std::vector<char>& dest) {
    bool     neg = false;
    uint64_t u   = value;
    if (value < 0) {
        neg = true;
        u   = -value;
    }
    char  s[30];
    char* ch = s;
    do {
        *ch++ = '0' + (u % 10);
        u /= 10;
    } while (u);
    if (neg)
        *ch++ = '-';
    std::reverse(s, ch);
    dest.push_back('"');
    dest.insert(dest.end(), s, ch);
    dest.push_back('"');
}

inline void to_json(eosio::name value, std::vector<char>& dest) {
    char buffer[13];
    auto end = value.write_as_string(buffer, buffer + sizeof(buffer));
    dest.push_back('"');
    dest.insert(dest.end(), buffer, end);
    dest.push_back('"');
}

inline void to_json(eosio::symbol_code value, std::vector<char>& dest) {
    char buffer[10];
    dest.push_back('"');
    append(dest, std::string_view{buffer, size_t(value.write_as_string(buffer, buffer + sizeof(buffer)) - buffer)});
    dest.push_back('"');
}

inline void to_json(eosio::asset value, std::vector<char>& dest) {
    append(dest, "{\"symbol\":\"");
    char buffer[10];
    append(dest, std::string_view{buffer, size_t(value.symbol.code().write_as_string(buffer, buffer + sizeof(buffer)) - buffer)});
    append(dest, "\",\"precision\":");
    to_json(value.symbol.precision(), dest);
    append(dest, ",\"amount\":");
    to_json(value.amount, dest);
    append(dest, "}");
}

inline void to_json(eosio::extended_asset value, std::vector<char>& dest) {
    append(dest, "{\"contract\":");
    to_json(value.contract, dest);
    append(dest, ",\"symbol\":\"");
    char buffer[10];
    append(dest, std::string_view{buffer, size_t(value.quantity.symbol.code().write_as_string(buffer, buffer + sizeof(buffer)) - buffer)});
    append(dest, "\",\"precision\":");
    to_json(value.quantity.symbol.precision(), dest);
    append(dest, ",\"amount\":");
    to_json(value.quantity.amount, dest);
    append(dest, "}");
}

// todo: fix const
// todo: move hex conversion to checksum256
inline void to_json(serial_wrapper<eosio::checksum256>& value, std::vector<char>& dest) {
    static const char hex_digits[] = "0123456789ABCDEF";
    auto              bytes        = reinterpret_cast<const unsigned char*>(value.value.data());
    dest.push_back('"');
    auto pos = dest.size();
    dest.resize(pos + 64);
    for (int i = 0; i < 32; ++i) {
        dest[pos++] = hex_digits[bytes[i] >> 4];
        dest[pos++] = hex_digits[bytes[i] & 15];
    }
    dest.push_back('"');
}

template <typename T>
inline void to_json(std::optional<T>& obj, std::vector<char>& dest) {
    if (obj)
        to_json(*obj, dest);
    else
        append(dest, "null");
}

template <typename T>
inline void to_json(std::vector<T>& obj, std::vector<char>& dest) {
    dest.push_back('[');
    bool first = true;
    for (auto& v : obj) {
        if (!first)
            dest.push_back(',');
        first = false;
        to_json(v, dest);
    }
    dest.push_back(']');
}

template <typename T>
inline void to_json(T& obj, std::vector<char>& dest) {
    dest.push_back('{');
    bool first = true;
    for_each_member(obj, [&](std::string_view member_name, auto& member) {
        if (!first)
            dest.push_back(',');
        first = false;
        to_json(member_name, dest);
        dest.push_back(':');
        to_json(member, dest);
    });
    dest.push_back('}');
}

template <typename T>
inline std::vector<char> to_json(T& obj) {
    std::vector<char> result;
    to_json(obj, result);
    return result;
}

// todo: const v
template <typename... NamedTypes>
inline void to_json(named_variant<NamedTypes...>& v, std::vector<char>& dest) {
    dest.push_back('[');
    to_json(named_variant<NamedTypes...>::keys[v.value.index()], dest);
    dest.push_back(',');
    std::visit([&](auto& x) { to_json(x, dest); }, v.value);
    dest.push_back(']');
}
