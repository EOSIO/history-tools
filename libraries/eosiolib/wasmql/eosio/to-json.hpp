// copyright defined in LICENSE.txt

#pragma once

#include <date/date.h>
#include <eosio/tagged-variant.hpp>
#include <eosio/temp-placeholders.hpp>
#include <eosiolib/time.hpp>
#include <vector>

namespace eosio {

// todo: replace
__attribute__((noinline)) inline void append(std::vector<char>& dest, std::string_view sv) {
    dest.insert(dest.end(), sv.begin(), sv.end());
}

template <typename T>
void to_json(const T& obj, std::vector<char>& dest);

// todo: escape
// todo: handle non-utf8
__attribute__((noinline)) inline void to_json(std::string_view sv, std::vector<char>& dest) {
    dest.push_back('"');
    dest.insert(dest.end(), sv.begin(), sv.end());
    dest.push_back('"');
}

__attribute__((noinline)) inline void to_json(bool value, std::vector<char>& dest) {
    static const char t[] = "true";
    static const char f[] = "false";
    if (value)
        dest.insert(dest.end(), std::begin(t), std::end(t) - 1);
    else
        dest.insert(dest.end(), std::begin(f), std::end(f) - 1);
}

// todo: unify decimal conversions

__attribute__((noinline)) inline void to_json(uint8_t value, std::vector<char>& dest) {
    char  s[4];
    char* ch = s;
    do {
        *ch++ = '0' + (value % 10);
        value /= 10;
    } while (value);
    std::reverse(s, ch);
    dest.insert(dest.end(), s, ch);
}

__attribute__((noinline)) inline void to_json(uint32_t value, std::vector<char>& dest) {
    char  s[20];
    char* ch = s;
    do {
        *ch++ = '0' + (value % 10);
        value /= 10;
    } while (value);
    std::reverse(s, ch);
    dest.insert(dest.end(), s, ch);
}

__attribute__((noinline)) inline void to_json(uint32_t value, int digits, std::vector<char>& dest) {
    char  s[20];
    char* ch = s;
    while (digits--) {
        *ch++ = '0' + (value % 10);
        value /= 10;
    };
    std::reverse(s, ch);
    dest.insert(dest.end(), s, ch);
}

inline void to_json(uint16_t value, std::vector<char>& dest) { return to_json(uint32_t(value), dest); }

__attribute__((noinline)) inline void to_json(int32_t value, std::vector<char>& dest) {
    if (value < 0) {
        dest.push_back('-');
        value = -value;
    }
    to_json(uint32_t(value), dest);
}

__attribute__((noinline)) inline void to_json(int64_t value, std::vector<char>& dest) {
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

__attribute__((noinline)) inline void to_json(name value, std::vector<char>& dest) {
    char buffer[13];
    auto end = value.write_as_string(buffer, buffer + sizeof(buffer));
    dest.push_back('"');
    dest.insert(dest.end(), buffer, end);
    dest.push_back('"');
}

__attribute__((noinline)) inline void to_json(symbol_code value, std::vector<char>& dest) {
    char buffer[10];
    dest.push_back('"');
    append(dest, std::string_view{buffer, size_t(value.write_as_string(buffer, buffer + sizeof(buffer)) - buffer)});
    dest.push_back('"');
}

__attribute__((noinline)) inline void to_json(asset value, std::vector<char>& dest) {
    append(dest, "{\"symbol\":\"");
    char buffer[10];
    append(dest, std::string_view{buffer, size_t(value.symbol.code().write_as_string(buffer, buffer + sizeof(buffer)) - buffer)});
    append(dest, "\",\"precision\":");
    to_json(value.symbol.precision(), dest);
    append(dest, ",\"amount\":");
    to_json(value.amount, dest);
    append(dest, "}");
}

__attribute__((noinline)) inline void to_json(extended_asset value, std::vector<char>& dest) {
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

// todo: move hex conversion to checksum256
__attribute__((noinline)) inline void to_json(const serial_wrapper<checksum256>& value, std::vector<char>& dest) {
    static const char hex_digits[] = "0123456789ABCDEF";
    auto              bytes        = value.value.extract_as_byte_array();
    dest.push_back('"');
    auto pos = dest.size();
    dest.resize(pos + 64);
    for (int i = 0; i < 32; ++i) {
        dest[pos++] = hex_digits[bytes[i] >> 4];
        dest[pos++] = hex_digits[bytes[i] & 15];
    }
    dest.push_back('"');
}

__attribute__((noinline)) inline void to_str_us(uint64_t microseconds, std::vector<char>& dest) {
    std::chrono::microseconds us{microseconds};
    date::sys_days            sd(std::chrono::floor<date::days>(us));
    auto                      ymd = date::year_month_day{sd};
    uint32_t                  ms  = (std::chrono::round<std::chrono::milliseconds>(us) - sd.time_since_epoch()).count();
    us -= sd.time_since_epoch();
    to_json((uint32_t)(int)ymd.year(), 4, dest);
    dest.push_back('-');
    to_json((uint32_t)(unsigned)ymd.month(), 2, dest);
    dest.push_back('-');
    to_json((uint32_t)(unsigned)ymd.day(), 2, dest);
    dest.push_back('T');
    to_json((uint32_t)ms / 3600000 % 60, 2, dest);
    dest.push_back(':');
    to_json((uint32_t)ms / 60000 % 60, 2, dest);
    dest.push_back(':');
    to_json((uint32_t)ms / 1000 % 60, 2, dest);
    dest.push_back('.');
    to_json((uint32_t)ms % 1000, 3, dest);
}

// todo: move conversion to time_point
__attribute__((noinline)) inline void to_json(time_point value, std::vector<char>& dest) {
    dest.push_back('"');
    to_str_us(value.elapsed.count(), dest);
    dest.push_back('"');
}

__attribute__((noinline)) inline void to_json(block_timestamp value, std::vector<char>& dest) { to_json(value.to_time_point(), dest); }

// todo
__attribute__((noinline)) inline void to_json(const datastream<const char*>& value, std::vector<char>& dest) {
    if (value.remaining())
        append(dest, "\"<<<datastream>>>\"");
    else
        append(dest, "\"<<<empty datastream>>>\"");
}

template <typename T>
__attribute__((noinline)) inline void to_json(const std::optional<T>& obj, std::vector<char>& dest) {
    if (obj)
        to_json(*obj, dest);
    else
        append(dest, "null");
}

template <typename T>
__attribute__((noinline)) inline void to_json(const std::vector<T>& obj, std::vector<char>& dest) {
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
__attribute__((noinline)) inline void to_json(const T& obj, std::vector<char>& dest) {
    dest.push_back('{');
    bool first = true;
    for_each_member((T*)nullptr, [&](std::string_view member_name, auto member) {
        if (!first)
            dest.push_back(',');
        first = false;
        to_json(member_name, dest);
        dest.push_back(':');
        to_json(member_from_void(member, &obj), dest);
    });
    dest.push_back('}');
}

template <typename T>
__attribute__((noinline)) inline std::vector<char> to_json(const T& obj) {
    std::vector<char> result;
    to_json(obj, result);
    return result;
}

template <tagged_variant_options Options, typename... NamedTypes>
__attribute__((noinline)) inline void to_json(const tagged_variant<Options, NamedTypes...>& v, std::vector<char>& dest) {
    dest.push_back('[');
    to_json(tagged_variant<Options, NamedTypes...>::keys[v.value.index()], dest);
    std::visit(
        [&](auto& x) {
            if constexpr (!is_named_empty_type_v<std::decay_t<decltype(x)>>) {
                dest.push_back(',');
                to_json(x, dest);
            }
        },
        v.value);
    dest.push_back(']');
}

template <typename T>
__attribute__((noinline)) void kv_to_json(std::string_view key, const T& value, std::vector<char>& dest) {
    to_json(key, dest);
    dest.push_back(':');
    to_json(value, dest);
}

} // namespace eosio
