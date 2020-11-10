// copyright defined in LICENSE.txt

#pragma once
#include "query_config.hpp"
#include "state_history.hpp"
#include <eosio/chain_conversions.hpp>
#include <eosio/check.hpp>
#include <eosio/from_string.hpp>
#include <eosio/time.hpp>
#include <eosio/varint.hpp>

#include <pqxx/pqxx>

namespace eosio {

template<>
inline time_point convert_from_string(std::string_view s) {
    uint64_t utc_microseconds;
    if (!string_to_utc_microseconds(utc_microseconds, s.data(), s.data() + s.size())) {
       check(false, "Expected time point in string conversion");
    }
    return time_point(eosio::microseconds(utc_microseconds));
}

}

namespace state_history {
namespace pg {

inline std::string null_value(bool bulk) {
    if (bulk)
        return "\\N";
    else
        return "null";
}

inline std::string sep(bool bulk) {
    if (bulk)
        return "\t";
    else
        return ",";
}

inline std::string quote(bool bulk, std::string s) {
    if (bulk)
        return s;
    else
        return "'" + s + "'";
}

inline std::string quote(std::string s) { return quote(false, s); }

inline std::string quote_bytea(bool bulk, std::string s) {
    if (bulk)
        return "\\\\x" + s;
    else
        return "'\\x" + s + "'";
}

inline std::string begin_array(bool bulk) {
    if (bulk)
        return "{";
    else
        return "array[";
}

inline std::string end_array(bool bulk, const std::string& type) {
    if (bulk)
        return "}";
    else
        return "]::" + type + "[]";
}

inline std::string end_array(bool bulk, pqxx::work& t, const std::string& schema, const std::string& type) {
    if (bulk)
        return "}";
    else
        return "]::" + t.quote_name(schema) + "." + t.quote_name(type) + "[]";
}

inline std::string begin_object_in_array(bool bulk) {
    if (bulk)
        return "\"(";
    else
        return "(";
}

inline std::string end_object_in_array(bool bulk) {
    if (bulk)
        return ")\"";
    else
        return ")";
}

inline abieos::bytes sql_to_bytes(const char* ch) {
    abieos::bytes result;
    if (!ch || ch[0] != '\\' || ch[1] != 'x')
        return result;
    std::string error;
    if (!abieos::unhex(error, ch + 2, ch + strlen(ch), std::back_inserter(result.data)))
        result.data.clear();
    return result;
}

inline abieos::checksum256 sql_to_checksum256(const char* ch) {
    if (!*ch)
        return {};
    std::vector<uint8_t> v;
    std::string          error;
    if (!abieos::unhex(error, ch, ch + strlen(ch), std::back_inserter(v)))
        throw std::runtime_error("expected hex string");
    abieos::checksum256 result;
    if (v.size() != result.value.size())
        throw std::runtime_error("hex string has incorrect length");
    memcpy(result.value.data(), v.data(), result.value.size());
    return result;
}

template <typename T>
std::string sql_str(pqxx::connection& c, bool bulk, const T& obj);

inline std::string sql_str(pqxx::connection& c, bool bulk, const std::string& s) {
    try {
        std::string tmp = c.esc(s);
        std::string result;
        result.reserve(tmp.size() + 2);
        if (!bulk)
            result += "'";
        for (auto ch : tmp) {
            if (ch == '\t')
                result += "\\t";
            else if (ch == '\r')
                result += "\\r";
            else if (ch == '\n')
                result += "\\n";
            else
                result += ch;
        }
        if (!bulk)
            result += "'";
        return result;
    } catch (...) {
        std::string result;
        if (!bulk)
            result = "'";
        abieos::hex(s.begin(), s.end(), back_inserter(result));
        if (!bulk)
            result += "'";
        return result;
    }
}

template <typename T>
std::string sql_str(bool bulk, const T& v);

// clang-format off
inline std::string sql_str(bool bulk, bool v)                                           { if(bulk) return v ? "t" : "f"; return v ? "true" : "false";}
inline std::string sql_str(bool bulk, uint8_t v)                                        { return std::to_string(v); }
inline std::string sql_str(bool bulk, int8_t v)                                         { return std::to_string(v); }
inline std::string sql_str(bool bulk, uint16_t v)                                       { return std::to_string(v); }
inline std::string sql_str(bool bulk, int16_t v)                                        { return std::to_string(v); }
inline std::string sql_str(bool bulk, uint32_t v)                                       { return std::to_string(v); }
inline std::string sql_str(bool bulk, int32_t v)                                        { return std::to_string(v); }
inline std::string sql_str(bool bulk, uint64_t v)                                       { return std::to_string(v); }
inline std::string sql_str(bool bulk, int64_t v)                                        { return std::to_string(v); }
inline std::string sql_str(bool bulk, eosio::varuint32 v)                               { return std::to_string(v.value); }
inline std::string sql_str(bool bulk, eosio::varint32 v)                                { return std::to_string(v.value); }
#ifndef ABIEOS_NO_INT128
inline std::string sql_str(bool bulk, const abieos::int128& v)                          { std::array<uint8_t, 128/8> t; auto nv = -v; return v < 0 ? std::string("-") + abieos::binary_to_decimal(eosio::fixed_bytes<128/8, uint8_t>([&](){std::memcpy(&t, &nv, sizeof(uint8_t)*128/8); return t;}()).get_array()) : abieos::binary_to_decimal(eosio::fixed_bytes<128/8, uint8_t>([&](){std::memcpy(&t, &v, sizeof(uint8_t)*128/8); return t;}()).get_array()); }
inline std::string sql_str(bool bulk, const abieos::uint128& v)                         { std::array<uint8_t, 128/8> t; std::memcpy(&t, &v, sizeof(uint8_t)*128/8); return abieos::binary_to_decimal(eosio::fixed_bytes<128/8, uint8_t>(t).get_array()); }
#else
inline std::string sql_str(bool bulk, const abieos::int128& v)                          { auto nv = v; abieos::negate(nv.data); return abieos::is_negative(v.data) ? std::string("-") + abieos::binary_to_decimal(eosio::fixed_bytes<128/8, uint8_t>(nv.data).get_array()) : abieos::binary_to_decimal(eosio::fixed_bytes<128/8, uint8_t>(v.data).get_array()); }
inline std::string sql_str(bool bulk, const abieos::uint128& v)                         { return abieos::binary_to_decimal(eosio::fixed_bytes<128/8, uint8_t>(v.data).get_array()); }
#endif
inline std::string sql_str(bool bulk, const abieos::float128& v)                        { return quote_bytea(bulk, abieos::hex(v.value.begin(), v.value.end())); }
inline std::string sql_str(bool bulk, eosio::name v)                                    { return quote(bulk, v.value ? std::string(v) : std::string()); }
inline std::string sql_str(bool bulk, eosio::time_point v)                              { return v.elapsed.count() ? quote(bulk, eosio::microseconds_to_str(v.elapsed.count())): null_value(bulk); }
inline std::string sql_str(bool bulk, eosio::time_point_sec v)                          { return v.utc_seconds ? quote(bulk, eosio::microseconds_to_str(uint64_t(v.utc_seconds) * 1'000'000)): null_value(bulk); }
inline std::string sql_str(bool bulk, abieos::block_timestamp v)                        { return v.slot ?  sql_str(bulk, v.to_time_point()) : null_value(bulk); }
inline std::string sql_str(bool bulk, const eosio::checksum256& v)                      { return quote(bulk, v.value == abieos::checksum256{}.value ? "" : abieos::hex(v.value.begin(), v.value.end())); }
inline std::string sql_str(bool bulk, const eosio::public_key& v)                       { return quote(bulk, public_key_to_string(v)); }
inline std::string sql_str(bool bulk, const eosio::signature& v)                        { return quote(bulk, signature_to_string(v)); }
inline std::string sql_str(bool bulk, const eosio::bytes&)                              { throw std::runtime_error("sql_str(bytes): not implemented"); }
inline std::string sql_str(bool bulk, eosio::ship_protocol::transaction_status v)       { return quote(bulk, to_string(v)); }
inline std::string sql_str(bool bulk, eosio::symbol v)                                  { return quote(bulk, eosio::symbol_to_string(v.value)); }

inline std::string sql_str(pqxx::connection&, bool bulk, bool v)                        { return sql_str(bulk, v); }
inline std::string sql_str(pqxx::connection&, bool bulk, eosio::varuint32 v)            { return sql_str(bulk, v); }
inline std::string sql_str(pqxx::connection&, bool bulk, eosio::varint32 v)             { return sql_str(bulk, v); }
inline std::string sql_str(pqxx::connection&, bool bulk, const abieos::int128& v)       { return sql_str(bulk, v); }
inline std::string sql_str(pqxx::connection&, bool bulk, const abieos::uint128& v)      { return sql_str(bulk, v); }
inline std::string sql_str(pqxx::connection&, bool bulk, const eosio::float128& v)      { return sql_str(bulk, v); }
inline std::string sql_str(pqxx::connection&, bool bulk, eosio::name v)                 { return sql_str(bulk, v); }
inline std::string sql_str(pqxx::connection&, bool bulk, eosio::time_point v)           { return sql_str(bulk, v); }
inline std::string sql_str(pqxx::connection&, bool bulk, eosio::time_point_sec v)       { return sql_str(bulk, v); }
inline std::string sql_str(pqxx::connection&, bool bulk, eosio::block_timestamp v)      { return sql_str(bulk, v); }
inline std::string sql_str(pqxx::connection&, bool bulk, eosio::checksum256 v)          { return sql_str(bulk, v); }
inline std::string sql_str(pqxx::connection&, bool bulk, const eosio::public_key& v)    { return sql_str(bulk, v); }
inline std::string sql_str(pqxx::connection&, bool bulk, const eosio::signature& v)     { return sql_str(bulk, v); }
inline std::string sql_str(pqxx::connection&, bool bulk,
                           eosio::ship_protocol::transaction_status v)                  { return sql_str(bulk, v); }
inline std::string sql_str(pqxx::connection&, bool bulk, eosio::symbol v)               { return sql_str(bulk, v); }
// clang-format on

template <typename T>
inline constexpr bool is_vector_v = false;

template <typename T>
inline constexpr bool is_vector_v<std::vector<T>> = true;

template <typename T>
inline constexpr bool is_optional_v = false;

template <typename T>
inline constexpr bool is_optional_v<std::optional<T>> = true;

template <typename T>
inline constexpr bool is_variant_v = false;

template <typename... Ts>
inline constexpr bool is_variant_v<std::variant<Ts...>> = true;

template <typename T>
inline constexpr bool is_string_v = false;

template <>
inline constexpr bool is_string_v<std::string> = true;

template <typename T>
std::string sql_str(pqxx::connection& c, bool bulk, const T& obj) {
    using std::to_string;
    if constexpr (is_optional_v<T>) {
        if (obj)
            return sql_str(c, bulk, *obj);
        else if (std::is_arithmetic_v<typename T::value_type>)
            return "0";
        else if (is_string_v<typename T::value_type>)
            return quote(bulk, "");
        else
            return null_value(bulk);
    } else {
        return to_string(obj);
    }
}

template <typename T>
std::string bin_to_sql(pqxx::connection& c, bool bulk, eosio::input_stream& bin) {
    if constexpr (is_optional_v<T>) {
        bool has_value;
        bin.read_raw<bool>(has_value);
        if (has_value)
            return bin_to_sql<typename T::value_type>(c, bulk, bin);
        else if (std::is_arithmetic_v<typename T::value_type>)
            return "0";
        else if (is_string_v<typename T::value_type>)
            return quote(bulk, "");
        else
            return null_value(bulk);
    } else {
        T v;
        bin.read_raw<T>(v);
        return sql_str(c, bulk, v);
    }
}

template <typename T>
std::string native_to_sql(pqxx::connection& c, bool bulk, const void* p) {
    return sql_str(c, bulk, *reinterpret_cast<const T*>(p));
}

template <typename T>
std::string empty_to_sql(pqxx::connection& c, bool bulk) {
    return sql_str(c, bulk, T{});
}

template <>
inline std::string bin_to_sql<std::string>(pqxx::connection& c, bool bulk, eosio::input_stream& bin) {
    std::string str;
    from_bin(str, bin);
    return sql_str(c, bulk, str);
}

template <>
inline std::string bin_to_sql<abieos::bytes>(pqxx::connection&, bool bulk, eosio::input_stream& bin) {
    uint32_t size;
    eosio::varuint32_from_bin(size, bin);
    eosio::check(size > bin.end - bin.pos, "invalid bytes size");
    std::string result;
    abieos::hex(bin.pos, bin.pos + size, back_inserter(result));
    bin.pos += size;
    return quote_bytea(bulk, result);
}

template <>
inline std::string native_to_sql<abieos::bytes>(pqxx::connection&, bool bulk, const void* p) {
    auto&       obj = reinterpret_cast<const abieos::bytes*>(p)->data;
    std::string result;
    abieos::hex(obj.data(), obj.data() + obj.size(), back_inserter(result));
    return quote_bytea(bulk, result);
}

template <>
inline std::string empty_to_sql<abieos::bytes>(pqxx::connection&, bool bulk) {
    return quote_bytea(bulk, "");
}

template <>
inline std::string bin_to_sql<eosio::input_stream>(pqxx::connection&, bool, eosio::input_stream& /*bin*/) {
    eosio::check(false, "bin_to_sql: input_buffer unsupported");
    return std::string{};
}

template <>
inline std::string native_to_sql<eosio::input_stream>(pqxx::connection&, bool bulk, const void* p) {
    auto&       obj = *reinterpret_cast<const eosio::input_stream*>(p);
    std::string result;
    abieos::hex(obj.pos, obj.end, back_inserter(result));
    return quote_bytea(bulk, result);
}

template <>
inline std::string empty_to_sql<eosio::input_stream>(pqxx::connection&, bool bulk) {
    return quote_bytea(bulk, "");
}

inline abieos::time_point sql_to_time_point(std::string s) {
    if (s.empty())
        return {};
    std::replace(s.begin(), s.end(), ' ', 'T');
    return eosio::convert_from_string<eosio::time_point>(s);
}

inline abieos::block_timestamp sql_to_block_timestamp(std::string s) {
    if (s.empty())
        return {};
    std::replace(s.begin(), s.end(), ' ', 'T');
    return abieos::block_timestamp{eosio::convert_from_string<eosio::time_point>(s)};
}

template <typename T>
void sql_to_bin(std::vector<char>& bin, const pqxx::field& f) {
    if constexpr (is_optional_v<T>)
        throw std::runtime_error("sql_to_bin<optional<T>> not implemented");
    else
        eosio::convert_to_bin(f.as<T>(), bin);
}

// clang-format off
template <>
inline void sql_to_bin<eosio::ship_protocol::transaction_status>(std::vector<char>& bin, const pqxx::field& f) {
    if (false) {}
    else if (!strcmp("executed",  f.c_str())) eosio::convert_to_bin( (uint8_t)eosio::ship_protocol::transaction_status::executed, bin);
    else if (!strcmp("soft_fail", f.c_str())) eosio::convert_to_bin( (uint8_t)eosio::ship_protocol::transaction_status::soft_fail, bin);
    else if (!strcmp("hard_fail", f.c_str())) eosio::convert_to_bin( (uint8_t)eosio::ship_protocol::transaction_status::hard_fail, bin);
    else if (!strcmp("delayed",   f.c_str())) eosio::convert_to_bin( (uint8_t)eosio::ship_protocol::transaction_status::delayed, bin);
    else if (!strcmp("expired",   f.c_str())) eosio::convert_to_bin( (uint8_t)eosio::ship_protocol::transaction_status::expired, bin);
    else
        throw std::runtime_error("invalid value for transaction_status: " + f.as<std::string>());
}
// clang-format on

// clang-format off
template <> inline void sql_to_bin<uint8_t>                    (std::vector<char>& bin, const pqxx::field& f) { eosio::convert_to_bin( (uint8_t)f.as<uint16_t>(), bin); }
template <> inline void sql_to_bin<int8_t>                     (std::vector<char>& bin, const pqxx::field& f) { eosio::convert_to_bin( (int8_t)f.as<int16_t>(), bin); }
template <> inline void sql_to_bin<abieos::varuint32>          (std::vector<char>& bin, const pqxx::field& f) { eosio::push_varuint32(bin, f.as<uint32_t>()); }
template <> inline void sql_to_bin<abieos::varint32>           (std::vector<char>& bin, const pqxx::field& f) { int32_t v = f.as<int32_t>(); eosio::push_varuint32(bin, (uint32_t(v) << 1) ^ uint32_t(v >> 31)); }
template <> inline void sql_to_bin<abieos::int128>             (std::vector<char>& bin, const pqxx::field& f) { throw std::runtime_error("sql_to_bin<int128> not implemented"); }
template <> inline void sql_to_bin<abieos::uint128>            (std::vector<char>& bin, const pqxx::field& f) { throw std::runtime_error("sql_to_bin<uint128> not implemented"); }
template <> inline void sql_to_bin<abieos::float128>           (std::vector<char>& bin, const pqxx::field& f) { throw std::runtime_error("sql_to_bin<float128> not implemented"); }
template <> inline void sql_to_bin<abieos::name>               (std::vector<char>& bin, const pqxx::field& f) { eosio::convert_to_bin( abieos::name{f.c_str()}, bin); }
template <> inline void sql_to_bin<abieos::time_point>         (std::vector<char>& bin, const pqxx::field& f) { eosio::convert_to_bin( sql_to_time_point(f.c_str()), bin); }
template <> inline void sql_to_bin<abieos::time_point_sec>     (std::vector<char>& bin, const pqxx::field& f) { throw std::runtime_error("sql_to_bin<time_point_sec> not implemented"); }
template <> inline void sql_to_bin<abieos::block_timestamp>    (std::vector<char>& bin, const pqxx::field& f) { eosio::convert_to_bin( sql_to_block_timestamp(f.c_str()), bin); }
template <> inline void sql_to_bin<abieos::checksum256>        (std::vector<char>& bin, const pqxx::field& f) { eosio::convert_to_bin( sql_to_checksum256(f.c_str()), bin); }
template <> inline void sql_to_bin<abieos::public_key>         (std::vector<char>& bin, const pqxx::field& f) { throw std::runtime_error("sql_to_bin<public_key> not implemented"); }
template <> inline void sql_to_bin<abieos::signature>          (std::vector<char>& bin, const pqxx::field& f) { throw std::runtime_error("sql_to_bin<signature> not implemented"); }
template <> inline void sql_to_bin<abieos::bytes>              (std::vector<char>& bin, const pqxx::field& f) { eosio::convert_to_bin( sql_to_bytes(f.c_str()), bin); }
template <> inline void sql_to_bin<std::string>                (std::vector<char>& bin, const pqxx::field& f) { eosio::convert_to_bin( std::string{f.c_str()}, bin); } // todo: unescape
template <> inline void sql_to_bin<eosio::input_stream>        (std::vector<char>& bin, const pqxx::field& f) { throw std::runtime_error("sql_to_bin<input_stream> not implemented"); }
template <> inline void sql_to_bin<abieos::symbol>             (std::vector<char>& bin, const pqxx::field& f) { uint64_t sym; eosio::check(eosio::string_to_symbol(sym, f.c_str(), f.c_str() + f.size() - 1), "sql_to_bin<symbol> failed to convert"); eosio::convert_to_bin(sym, bin); }
// clang-format on

struct type {
    const char* name                                                          = "";
    std::string (*bin_to_sql)(pqxx::connection&, bool, eosio::input_stream&)  = nullptr;
    std::string (*native_to_sql)(pqxx::connection&, bool, const void*)        = nullptr;
    std::string (*empty_to_sql)(pqxx::connection&, bool)                      = nullptr;
    void (*sql_to_bin)(std::vector<char>& bin, const pqxx::field&)            = nullptr;
};

template <typename T>
struct unknown_type {};

inline constexpr bool is_known_type(type) { return true; }

template <typename T>
inline constexpr bool is_known_type(unknown_type<T>) {
    return false;
}

template <typename T>
inline constexpr unknown_type<T> type_for;

template <typename T>
constexpr type make_type_for(const char* name) {
    return type{name, bin_to_sql<T>, native_to_sql<T>, empty_to_sql<T>, sql_to_bin<T>};
}

// clang-format off
template<> inline constexpr type type_for<bool>                     = make_type_for<bool>(                      "bool"                      );
template<> inline constexpr type type_for<uint8_t>                  = make_type_for<uint8_t>(                   "smallint"                  );
template<> inline constexpr type type_for<int8_t>                   = make_type_for<int8_t>(                    "smallint"                  );
template<> inline constexpr type type_for<uint16_t>                 = make_type_for<uint16_t>(                  "integer"                   );
template<> inline constexpr type type_for<int16_t>                  = make_type_for<int16_t>(                   "smallint"                  );
template<> inline constexpr type type_for<uint32_t>                 = make_type_for<uint32_t>(                  "bigint"                    );
template<> inline constexpr type type_for<int32_t>                  = make_type_for<int32_t>(                   "integer"                   );
template<> inline constexpr type type_for<uint64_t>                 = make_type_for<uint64_t>(                  "decimal"                   );
template<> inline constexpr type type_for<int64_t>                  = make_type_for<int64_t>(                   "bigint"                    );
template<> inline constexpr type type_for<abieos::uint128>          = make_type_for<abieos::uint128>(           "decimal"                   );
template<> inline constexpr type type_for<abieos::int128>           = make_type_for<abieos::int128>(            "decimal"                   );
template<> inline constexpr type type_for<double>                   = make_type_for<double>(                    "float8"                    );
template<> inline constexpr type type_for<abieos::float128>         = make_type_for<abieos::float128>(          "bytea"                     );
template<> inline constexpr type type_for<abieos::varuint32>        = make_type_for<abieos::varuint32>(         "bigint"                    );
template<> inline constexpr type type_for<abieos::varint32>         = make_type_for<abieos::varint32>(          "integer"                   );
template<> inline constexpr type type_for<abieos::name>             = make_type_for<abieos::name>(              "varchar(13)"               );
template<> inline constexpr type type_for<abieos::checksum256>      = make_type_for<abieos::checksum256>(       "varchar(64)"               );
template<> inline constexpr type type_for<std::string>              = make_type_for<std::string>(               "varchar"                   );
template<> inline constexpr type type_for<abieos::time_point>       = make_type_for<abieos::time_point>(        "timestamp"                 );
template<> inline constexpr type type_for<abieos::time_point_sec>   = make_type_for<abieos::time_point_sec>(    "timestamp"                 );
template<> inline constexpr type type_for<abieos::block_timestamp>  = make_type_for<abieos::block_timestamp>(   "timestamp"                 );
template<> inline constexpr type type_for<abieos::public_key>       = make_type_for<abieos::public_key>(        "varchar"                   );
template<> inline constexpr type type_for<abieos::signature>        = make_type_for<abieos::signature>(         "varchar"                   );
template<> inline constexpr type type_for<abieos::bytes>            = make_type_for<abieos::bytes>(             "bytea"                     );
template<> inline constexpr type type_for<eosio::input_stream>      = make_type_for<eosio::input_stream>(       "bytea"                     );
template<> inline constexpr type type_for<abieos::symbol>           = make_type_for<abieos::symbol>(            "varchar(10)"               );
template<> inline constexpr type type_for<eosio::ship_protocol::transaction_status>
                                                                    = make_type_for<eosio::ship_protocol::transaction_status>("transaction_status_type");
// clang-format on

template <typename T>
inline constexpr auto make_optional_type_for() {
    if constexpr (is_known_type(type_for<T>))
        return make_type_for<std::optional<T>>(type_for<T>.name);
    else
        return unknown_type<std::optional<T>>{};
}

template <typename T>
inline constexpr auto type_for<std::optional<T>> = make_optional_type_for<T>();

// clang-format off
inline const std::map<std::string_view, type> abi_type_to_sql_type = {
    {"bool",                    type_for<bool>},
    {"uint8",                   type_for<uint8_t>},
    {"int8",                    type_for<int8_t>},
    {"uint16",                  type_for<uint16_t>},
    {"int16",                   type_for<int16_t>},
    {"uint32",                  type_for<uint32_t>},
    {"uint32?",                 type_for<std::optional<uint32_t>>},
    {"int32",                   type_for<int32_t>},
    {"uint64",                  type_for<uint64_t>},
    {"int64",                   type_for<int64_t>},
    {"uint128",                 type_for<abieos::uint128>},
    {"int128",                  type_for<abieos::int128>},
    {"float64",                 type_for<double>},
    {"float128",                type_for<abieos::float128>},
    {"varuint32",               type_for<abieos::varuint32>},
    {"varint32",                type_for<abieos::varint32>},
    {"name",                    type_for<abieos::name>},
    {"checksum256",             type_for<abieos::checksum256>},
    {"string",                  type_for<std::string>},
    {"time_point",              type_for<abieos::time_point>},
    {"time_point_sec",          type_for<abieos::time_point_sec>},
    {"block_timestamp_type",    type_for<abieos::block_timestamp>},
    {"public_key",              type_for<abieos::public_key>},
    {"signature",               type_for<abieos::signature>},
    {"bytes",                   type_for<abieos::bytes>},
    {"transaction_status",      type_for<eosio::ship_protocol::transaction_status>},
    {"symbol",                  type_for<abieos::symbol>},
};

// clang-format on

struct defs {
    using type   = pg::type;
    using field  = query_config::field<defs>;
    using key    = query_config::key<defs>;
    using table  = query_config::table<defs>;
    using index  = query_config::index<defs>;
    using query  = query_config::query<defs>;
    using config = query_config::config<defs>;
}; // defs

using field  = defs::field;
using key    = defs::key;
using table  = defs::table;
using index  = defs::index;
using query  = defs::query;
using config = defs::config;

} // namespace pg
} // namespace state_history
