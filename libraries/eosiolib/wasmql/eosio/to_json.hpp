// copyright defined in LICENSE.txt

#pragma once

#include <date/date.h>
#include <eosio/rope.hpp>
#include <eosio/tagged_variant.hpp>
#include <eosio/temp_placeholders.hpp>
#include <eosio/time.hpp>
#include <limits>
#include <vector>

namespace eosio {

/// \exclude
namespace internal_use_do_not_use {

// CDT behaviors:
// * malloc() never returns nullptr; it asserts
// * free() does nothing
// * eosio::rope holds onto raw pointers and doesn't free()
struct rope_buffer {
    char* begin;
    char* pos;
    char* end;

    rope_buffer(size_t size)
        : begin{(char*)malloc(size)}
        , pos{begin}
        , end(begin + size) {}

    operator rope() { return rope{std::string_view{begin, size_t(pos - begin)}}; }

    void reverse() { std::reverse(begin, pos); }
};

} // namespace internal_use_do_not_use

/// \exclude
template <typename T>
rope to_json(const T& obj);

// todo: escape
// todo: handle non-utf8
/// \group to_json_explicit Convert explicit types to JSON
/// Convert objects to JSON. These overloads handle specified types.
__attribute__((noinline)) inline rope to_json(std::string_view sv) { return rope{"\""} + sv + "\""; }

/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(bool value) {
    if (value)
        return "true";
    else
        return "false";
}

/// \exclude
template <typename T>
__attribute__((noinline)) inline rope int_to_json(T value) {
    using namespace internal_use_do_not_use;
    rope_buffer b(std::numeric_limits<T>::digits10 + 4);
    bool        neg = value < 0;
    if (neg)
        value = -value;
    if (sizeof(T) > 4)
        *b.pos++ = '"';
    do {
        *b.pos++ = '0' + (value % 10);
        value /= 10;
    } while (value);
    if (neg)
        *b.pos++ = '-';
    if (sizeof(T) > 4)
        *b.pos++ = '"';
    b.reverse();
    return b;
}

/// \exclude
template <typename T>
__attribute__((noinline)) inline rope uint_to_json_fixed_size(T value, unsigned digits) {
    using namespace internal_use_do_not_use;
    rope_buffer b(digits + 2);
    if (sizeof(T) > 4)
        *b.pos++ = '"';
    while (digits--) {
        *b.pos++ = '0' + (value % 10);
        value /= 10;
    };
    if (sizeof(T) > 4)
        *b.pos++ = '"';
    b.reverse();
    return b;
}

/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(uint8_t value) { return int_to_json(value); }

/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(uint16_t value) { return int_to_json(value); }

/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(uint32_t value) { return int_to_json(value); }

/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(uint64_t value) { return int_to_json(value); }

/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(int8_t value) { return int_to_json(value); }

/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(int16_t value) { return int_to_json(value); }

/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(int32_t value) { return int_to_json(value); }

/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(int64_t value) { return int_to_json(value); }

/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(name value) {
    using namespace internal_use_do_not_use;
    rope_buffer b{15};
    *b.pos++ = '"';
    b.pos    = value.write_as_string(b.pos, b.end - 1);
    *b.pos++ = '"';
    return b;
}

/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(symbol_code value) {
    using namespace internal_use_do_not_use;
    rope_buffer b{10};
    *b.pos++ = '"';
    b.pos    = value.write_as_string(b.pos, b.end - 1);
    *b.pos++ = '"';
    return b;
}

/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(asset value) {
    return rope{"{\"symbol\":"} +              //
           to_json(value.symbol.code()) +      //
           ",\"precision\":" +                 //
           to_json(value.symbol.precision()) + //
           ",\"amount\":" +                    //
           to_json(value.amount) +             //
           "}";
}

/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(extended_asset value) {
    return rope{"{\"contract\":"} +                     //
           to_json(value.contract) +                    //
           ",\"symbol\":" +                             //
           to_json(value.quantity.symbol.code()) +      //
           ",\"precision\":" +                          //
           to_json(value.quantity.symbol.precision()) + //
           ",\"amount\":" +                             //
           to_json(value.quantity.amount) +             //
           "}";
}

// todo: move hex conversion to checksum256
/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(const checksum256& value) {
    using namespace internal_use_do_not_use;
    static const char hex_digits[] = "0123456789ABCDEF";
    auto              bytes        = value.extract_as_byte_array();
    rope_buffer       b{66};
    *b.pos++ = '"';
    for (int i = 0; i < 32; ++i) {
        *b.pos++ = hex_digits[bytes[i] >> 4];
        *b.pos++ = hex_digits[bytes[i] & 15];
    }
    *b.pos++ = '"';
    return b;
}

// todo: move conversion to time_point
/// \exclude
__attribute__((noinline)) inline rope to_str_us(uint64_t microseconds) {
    std::chrono::microseconds us{microseconds};
    date::sys_days            sd(std::chrono::floor<date::days>(us));
    auto                      ymd = date::year_month_day{sd};
    uint32_t                  ms  = (std::chrono::round<std::chrono::milliseconds>(us) - sd.time_since_epoch()).count();
    us -= sd.time_since_epoch();
    return uint_to_json_fixed_size((uint32_t)(int)ymd.year(), 4) +       //
           "-" +                                                         //
           uint_to_json_fixed_size((uint32_t)(unsigned)ymd.month(), 2) + //
           "-" +                                                         //
           uint_to_json_fixed_size((uint32_t)(unsigned)ymd.day(), 2) +   //
           "T" +                                                         //
           uint_to_json_fixed_size((uint32_t)ms / 3600000 % 60, 2) +     //
           ":" +                                                         //
           uint_to_json_fixed_size((uint32_t)ms / 60000 % 60, 2) +       //
           ":" +                                                         //
           uint_to_json_fixed_size((uint32_t)ms / 1000 % 60, 2) +        //
           "." +                                                         //
           uint_to_json_fixed_size((uint32_t)ms % 1000, 3);
}

/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(time_point value) { return rope{"\""} + to_str_us(value.elapsed.count()) + "\""; }

/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(block_timestamp value) { return to_json(value.to_time_point()); }

// todo
/// \group to_json_explicit
__attribute__((noinline)) inline rope to_json(const datastream<const char*>& value) {
    if (value.remaining())
        return "\"<<<datastream>>>\"";
    else
        return "\"<<<empty datastream>>>\"";
}

/// \group to_json_explicit
template <typename T>
__attribute__((noinline)) inline rope to_json(const std::optional<T>& obj) {
    if (obj)
        return to_json(*obj);
    else
        return "null";
}

/// \group to_json_explicit
template <typename T>
__attribute__((noinline)) inline rope to_json(const std::vector<T>& obj) {
    rope result = "[";
    bool first  = true;
    for (auto& v : obj) {
        if (!first)
            result += ",";
        first = false;
        result += to_json(v);
    }
    result += "]";
    return result;
}

/// \output_section Convert reflected objects to JSON
/// Convert an object to JSON. This overload works with
/// [reflected objects](standardese://reflection/).
template <typename T>
__attribute__((noinline)) inline rope to_json(const T& obj) {
    rope result = "{";
    bool first  = true;
    for_each_member((T*)nullptr, [&](std::string_view member_name, auto member) {
        if (!first)
            result += ",";
        first = false;
        result += to_json(member_name);
        result += ":";
        result += to_json(member_from_void(member, &obj));
    });
    result += "}";
    return result;
}

/// \group to_json_explicit
template <tagged_variant_options Options, typename... NamedTypes>
__attribute__((noinline)) inline rope to_json(const tagged_variant<Options, NamedTypes...>& v) {
    rope result = "[";
    result += to_json(tagged_variant<Options, NamedTypes...>::keys[v.value.index()]);
    std::visit(
        [&](auto& x) {
            if constexpr (!is_named_empty_type_v<std::decay_t<decltype(x)>>) {
                result += ",";
                result += to_json(x);
            }
        },
        v.value);
    result += "]";
    return result;
}

/// \output_section JSON Conversion Helpers
template <typename T>
__attribute__((noinline)) rope kv_to_json(std::string_view key, const T& value) {
    return to_json(key) + ":" + to_json(value);
}

} // namespace eosio
