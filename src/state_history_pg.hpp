// copyright defined in LICENSE.txt

#pragma once
#include "query_config.hpp"
#include "state_history.hpp"
#include <eosio/abi.hpp>
#include <eosio/chain_conversions.hpp>
#include <eosio/check.hpp>
#include <eosio/from_string.hpp>
#include <eosio/time.hpp>
#include <eosio/varint.hpp>
#include <eosio/to_json.hpp>
#include <pqxx/pqxx>
#include <pqxx/tablewriter.hxx>
#include <boost/algorithm/hex.hpp>


namespace eosio {

template <>
inline abi_type* add_type(abi& a, std::vector<ship_protocol::recurse_transaction_trace>*) {
    abi_type& element_type =
        a.abi_types.try_emplace("recurse_transaction_trace", "recurse_transaction_trace", abi_type::builtin{}, nullptr).first->second;
    std::string name      = "recurse_transaction_trace?";
    auto [iter, inserted] = a.abi_types.try_emplace(name, name, abi_type::optional{&element_type}, optional_abi_serializer);
    return &iter->second;
}

} // namespace eosio

namespace state_history {
namespace pg {

inline std::string quote_bytea(std::string s) { return "\\\\x" + s; }

inline eosio::checksum256 sql_to_checksum256(const char* ch) {
    if (!*ch)
        return {};
    std::vector<uint8_t> v;
    boost::algorithm::unhex(ch, ch + strlen(ch), std::back_inserter(v));
    eosio::checksum256 result;
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
    if (v.value != eosio::checksum256{}.value) {
        const auto& bytes = v.extract_as_byte_array();
        boost::algorithm::hex(bytes.begin(), bytes.end(), std::back_inserter(result));
    }
    return result;
}

inline std::string sql_str(const eosio::float128& v) {
    const auto& bytes = v.extract_as_byte_array();
    std::string r;
    boost::algorithm::hex(bytes.begin(), bytes.end(), std::back_inserter(r));
    return quote_bytea(r);
}

inline std::string sql_str(const eosio::ship_protocol::recurse_transaction_trace& v) {
    return sql_str(std::visit([](auto& x) { return x.id; }, v.recurse));
}

inline std::string sql_str(const __int128& v) {
    const int digits10 = 38;
    char      buf[digits10 + 2];
    char* end = eosio::int_to_decimal(v, buf);
    *end      = '\0';
    return buf;
}

inline std::string sql_str(const unsigned __int128& v) {
    const int digits10 = 38;
    char  buf[digits10 + 2];
    char* end = eosio::int_to_decimal(v, buf);
    *end      = '\0';
    return buf;
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
inline std::string sql_str(eosio::name v)                                    { return v.value ? std::string(v) : std::string(); }
inline std::string sql_str(eosio::time_point v)                              { return v.elapsed.count() ? eosio::microseconds_to_str(v.elapsed.count()): ""; }
inline std::string sql_str(eosio::time_point_sec v)                          { return v.utc_seconds ? eosio::microseconds_to_str(uint64_t(v.utc_seconds) * 1'000'000): ""; }
inline std::string sql_str(eosio::block_timestamp v)                        { return v.slot ?  sql_str(v.to_time_point()) : ""; }
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
inline std::string bin_to_sql<eosio::bytes>(eosio::input_stream& bin) {
    uint32_t size;
    eosio::varuint32_from_bin(size, bin);
    eosio::check(size <= bin.end - bin.pos, "invalid bytes size");
    std::string result;
    boost::algorithm::hex(bin.pos, bin.pos + size, back_inserter(result));
    bin.pos += size;
    return quote_bytea(result);
}

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
template<> inline constexpr type_names names_for<unsigned __int128>                               = type_names{"uint128","decimal"};
template<> inline constexpr type_names names_for<__int128>                                        = type_names{"int128","decimal"};
template<> inline constexpr type_names names_for<double>                                          = type_names{"float64","float8"};
template<> inline constexpr type_names names_for<eosio::float128>                                 = type_names{"float128","bytea"};
template<> inline constexpr type_names names_for<eosio::varuint32>                                = type_names{"varuint32","bigint"};
template<> inline constexpr type_names names_for<eosio::varint32>                                 = type_names{"varint32","integer"};
template<> inline constexpr type_names names_for<eosio::name>                                     = type_names{"name","varchar(13)"};
template<> inline constexpr type_names names_for<eosio::checksum256>                              = type_names{"checksum256","varchar(64)"};
template<> inline constexpr type_names names_for<std::string>                                     = type_names{"string","varchar"};
template<> inline constexpr type_names names_for<eosio::time_point>                               = type_names{"time_point","timestamp"};
template<> inline constexpr type_names names_for<eosio::time_point_sec>                           = type_names{"time_point_sec","timestamp"};
template<> inline constexpr type_names names_for<eosio::block_timestamp>                          = type_names{"block_timestamp_type","timestamp"};
template<> inline constexpr type_names names_for<eosio::public_key>                               = type_names{"public_key","varchar"};
template<> inline constexpr type_names names_for<eosio::signature>                                = type_names{"signature","varchar"};
template<> inline constexpr type_names names_for<eosio::bytes>                                    = type_names{"bytes","bytea"};
template<> inline constexpr type_names names_for<eosio::symbol>                                   = type_names{"symbol","varchar(10)"};
template<> inline constexpr type_names names_for<eosio::ship_protocol::transaction_status>        = type_names{"transaction_status","transaction_status_type"};
template<> inline constexpr type_names names_for<eosio::ship_protocol::recurse_transaction_trace> = type_names{"recurse_transaction_trace","varchar"};
// clang-format on

} // namespace pg
} // namespace state_history
