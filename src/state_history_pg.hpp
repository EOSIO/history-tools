// copyright defined in LICENSE.txt

#pragma once
#include "abieos.hpp"
#include "query_config.hpp"
#include "state_history.hpp"
#include <eosio/abi.hpp>
#include <eosio/chain_conversions.hpp>
#include <eosio/check.hpp>
#include <eosio/from_string.hpp>
#include <eosio/time.hpp>
#include <eosio/varint.hpp>
#include <pqxx/pqxx>
#include <pqxx/tablewriter.hxx>

namespace eosio {

template <>
inline abi_type* add_type(abi& a, std::vector<ship_protocol::recurse_transaction_trace>*) {
    abi_type& element_type =
        a.abi_types.try_emplace("recurse_transaction_trace", "recurse_transaction_trace", abi_type::builtin{}, nullptr).first->second;
    std::string name      = "recurse_transaction_trace?";
    auto [iter, inserted] = a.abi_types.try_emplace(name, name, abi_type::optional{&element_type}, optional_abi_serializer);
    return &iter->second;
}

template <>
inline time_point convert_from_string(std::string_view s) {
    uint64_t utc_microseconds;
    if (!string_to_utc_microseconds(utc_microseconds, s.data(), s.data() + s.size())) {
        check(false, "Expected time point in string conversion");
    }
    return time_point(eosio::microseconds(utc_microseconds));
}

} // namespace eosio

namespace state_history {
namespace pg {

inline std::string quote_bytea(std::string s) { return "\\\\x" + s; }

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
std::string sql_str(const T& obj);

inline std::string sql_str(const std::string& s) {
    return s;
}

inline std::string sql_str(const eosio::checksum256& v) {
    std::string result;
    if (v.value != abieos::checksum256{}.value) {
        const auto& bytes = v.extract_as_byte_array();
        result            = abieos::hex(bytes.begin(), bytes.end());
    }
    return result;
}

inline std::string sql_str(const abieos::float128& v) {
    const auto& bytes = v.extract_as_byte_array();
    return quote_bytea(abieos::hex(bytes.begin(), bytes.end()));
}

inline std::string sql_str(const eosio::ship_protocol::recurse_transaction_trace& v) {
    return sql_str(std::visit([](auto& x) { return x.id; }, v.recurse));
}

template <typename T>
std::string sql_str(const T& v);

// clang-format off
inline std::string sql_str(bool v)                                           { return v ? "true" : "false";}
inline std::string sql_str(uint8_t v)                                        { return std::to_string(v); }
inline std::string sql_str(int8_t v)                                         { return std::to_string(v); }
inline std::string sql_str(uint16_t v)                                       { return std::to_string(v); }
inline std::string sql_str(int16_t v)                                        { return std::to_string(v); }
inline std::string sql_str(uint32_t v)                                       { return std::to_string(v); }
inline std::string sql_str(int32_t v)                                        { return std::to_string(v); }
inline std::string sql_str(uint64_t v)                                       { return std::to_string(v); }
inline std::string sql_str(int64_t v)                                        { return std::to_string(v); }
inline std::string sql_str(double v)                                         { return std::to_string(v); }
inline std::string sql_str(eosio::varuint32 v)                               { return std::to_string(v.value); }
inline std::string sql_str(eosio::varint32 v)                                { return std::to_string(v.value); }
#ifndef ABIEOS_NO_INT128
inline std::string sql_str(const abieos::int128& v)                          { std::array<uint8_t, 128/8> t; auto nv = -v; return v < 0 ? std::string("-") + abieos::binary_to_decimal(*(std::array<uint8_t, 128/8>*)std::memcpy(&t, &nv, sizeof(uint8_t)*128/8)) : abieos::binary_to_decimal(*(std::array<uint8_t, 128/8>*)std::memcpy(&t, &v, sizeof(uint8_t)*128/8)); }
inline std::string sql_str(const abieos::uint128& v)                         { std::array<uint8_t, 128/8> t; std::memcpy(&t, &v, sizeof(uint8_t)*128/8); return abieos::binary_to_decimal(t); }
#else
inline std::string sql_str(const abieos::int128& v)                          { auto nv = v; abieos::negate(nv.data); return abieos::is_negative(v.data) ? std::string("-") + abieos::binary_to_decimal(nv.data) : abieos::binary_to_decimal(v.data); }
inline std::string sql_str(const abieos::uint128& v)                         { return abieos::binary_to_decimal(v.data); }
#endif
inline std::string sql_str(eosio::name v)                                    { return v.value ? std::string(v) : std::string(); }
inline std::string sql_str(eosio::time_point v)                              { return v.elapsed.count() ? eosio::microseconds_to_str(v.elapsed.count()): "\\N"; }
inline std::string sql_str(eosio::time_point_sec v)                          { return v.utc_seconds ? eosio::microseconds_to_str(uint64_t(v.utc_seconds) * 1'000'000): "\\N"; }
inline std::string sql_str(abieos::block_timestamp v)                        { return v.slot ?  sql_str(v.to_time_point()) : "\\N"; }
inline std::string sql_str(const eosio::public_key& v)                       { return public_key_to_string(v); }
inline std::string sql_str(const eosio::signature& v)                        { return signature_to_string(v); }
inline std::string sql_str(const eosio::bytes&)                              { throw std::runtime_error("sql_str(bytes): not implemented"); }
inline std::string sql_str(eosio::ship_protocol::transaction_status v)       { return to_string(v); }
inline std::string sql_str(eosio::symbol v)                                  { return eosio::symbol_to_string(v.value); }

// clang-format on

template <typename T>
std::string bin_to_sql(eosio::input_stream& bin) {
    T v;
    from_bin(v, bin);
    return sql_str(v);
}

template <>
inline std::string bin_to_sql<abieos::bytes>(eosio::input_stream& bin) {
    uint32_t size;
    eosio::varuint32_from_bin(size, bin);
    eosio::check(size <= bin.end - bin.pos, "invalid bytes size");
    std::string result;
    abieos::hex(bin.pos, bin.pos + size, back_inserter(result));
    bin.pos += size;
    return quote_bytea(result);
}

struct type {
    const char* name                                               = "";
    std::string (*bin_to_sql)(eosio::input_stream&)                = nullptr;
};

struct type_names {
    const char *abi, *sql;
};

// clang-format off
template <typename T>
type_names names_for;

template<> inline constexpr type_names names_for<bool>                                            = type_names{"bool", "bool"};
template<> inline constexpr type_names names_for<uint8_t>                                         = type_names{"uint8", "smallint"};
template<> inline constexpr type_names names_for<int8_t>                                          = type_names{"int8", "smallint"};
template<> inline constexpr type_names names_for<uint16_t>                                        = type_names{"uint16", "integer"};
template<> inline constexpr type_names names_for<int16_t>                                         = type_names{"int16", "smallint"};
template<> inline constexpr type_names names_for<uint32_t>                                        = type_names{"uint32","bigint"};
template<> inline constexpr type_names names_for<int32_t>                                         = type_names{"int32","integer"};
template<> inline constexpr type_names names_for<uint64_t>                                        = type_names{"uint64","decimal"};
template<> inline constexpr type_names names_for<int64_t>                                         = type_names{"int64","bigint"};
template<> inline constexpr type_names names_for<abieos::uint128>                                 = type_names{"uint128","decimal"};
template<> inline constexpr type_names names_for<abieos::int128>                                  = type_names{"int128","decimal"};
template<> inline constexpr type_names names_for<double>                                          = type_names{"float64","float8"};
template<> inline constexpr type_names names_for<abieos::float128>                                = type_names{"float128","bytea"};
template<> inline constexpr type_names names_for<abieos::varuint32>                               = type_names{"varuint32","bigint"};
template<> inline constexpr type_names names_for<abieos::varint32>                                = type_names{"varint32","integer"};
template<> inline constexpr type_names names_for<abieos::name>                                    = type_names{"name","varchar(13)"};
template<> inline constexpr type_names names_for<abieos::checksum256>                             = type_names{"checksum256","varchar(64)"};
template<> inline constexpr type_names names_for<std::string>                                     = type_names{"string","varchar"};
template<> inline constexpr type_names names_for<abieos::time_point>                              = type_names{"time_point","timestamp"};
template<> inline constexpr type_names names_for<abieos::time_point_sec>                          = type_names{"time_point_sec","timestamp"};
template<> inline constexpr type_names names_for<abieos::block_timestamp>                         = type_names{"block_timestamp_type","timestamp"};
template<> inline constexpr type_names names_for<abieos::public_key>                              = type_names{"public_key","varchar"};
template<> inline constexpr type_names names_for<abieos::signature>                               = type_names{"signature","varchar"};
template<> inline constexpr type_names names_for<abieos::bytes>                                   = type_names{"bytes","bytea"};
template<> inline constexpr type_names names_for<abieos::symbol>                                  = type_names{"symbol","varchar(10)"};
template<> inline constexpr type_names names_for<eosio::ship_protocol::transaction_status>        = type_names{"transaction_status","transaction_status_type"};
template<> inline constexpr type_names names_for<eosio::ship_protocol::recurse_transaction_trace> = type_names{"recurse_transaction_trace","varchar"};
// clang-format on

template <typename... input_t>
using tuple_cat_t = decltype(std::tuple_cat(std::declval<input_t>()...));

using basic_ship_types = std::tuple<
    bool, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t, double, std::string, abieos::uint128, abieos::int128,
    abieos::float128, abieos::varuint32, abieos::varint32, abieos::name, abieos::checksum256, abieos::time_point, abieos::time_point_sec,
    abieos::block_timestamp, abieos::public_key, abieos::signature, abieos::bytes, abieos::symbol, eosio::ship_protocol::transaction_status,
    eosio::ship_protocol::recurse_transaction_trace>;

inline std::map<std::string_view, type> make_basic_converter() {
    std::map<std::string_view, type> result;
    std::apply([&result](auto... x) {
        (result.try_emplace(names_for<decltype(x)>.abi, type{names_for<decltype(x)>.sql, bin_to_sql<decltype(x)>}),
         ...);
    }, basic_ship_types{});
    return result;
}

const auto abi_type_to_sql_type = make_basic_converter();

} // namespace pg
} // namespace state_history
