// copyright defined in LICENSE.txt

#include "abieos_exception.hpp"

#include <pqxx/pqxx>

namespace sql_conversion {

enum class transaction_status : uint8_t {
    executed  = 0, // succeed, no error handler executed
    soft_fail = 1, // objectively failed (not executed), error handler executed
    hard_fail = 2, // objectively failed and error handler objectively failed thus no state change
    delayed   = 3, // transaction delayed/deferred/scheduled for future execution
    expired   = 4, // transaction expired and storage space refunded to user
};

inline const std::string null_value = "null";
inline const std::string sep        = ",";

inline std::string quote(std::string s) { return "'" + s + "'"; }
inline std::string quote_bytea(std::string s) { return "'\\x" + s + "'"; }

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

// clang-format off
inline std::string sql_str(bool v)                          { return v ? "true" : "false";}
inline std::string sql_str(uint8_t v)                       { return std::to_string(v); }
inline std::string sql_str(int8_t v)                        { return std::to_string(v); }
inline std::string sql_str(uint16_t v)                      { return std::to_string(v); }
inline std::string sql_str(int16_t v)                       { return std::to_string(v); }
inline std::string sql_str(uint32_t v)                      { return std::to_string(v); }
inline std::string sql_str(int32_t v)                       { return std::to_string(v); }
inline std::string sql_str(uint64_t v)                      { return std::to_string(v); }
inline std::string sql_str(int64_t v)                       { return std::to_string(v); }
inline std::string sql_str(abieos::varuint32 v)             { return std::string(v); }
inline std::string sql_str(abieos::varint32 v)              { return std::string(v); }
inline std::string sql_str(const abieos::int128& v)         { return std::string(v); }
inline std::string sql_str(const abieos::uint128& v)        { return std::string(v); }
inline std::string sql_str(const abieos::float128& v)       { return quote_bytea(std::string(v)); }
inline std::string sql_str(abieos::name v)                  { return quote(v.value ? std::string(v) : ""); }
inline std::string sql_str(abieos::time_point v)            { return v.microseconds ? quote(std::string(v)): null_value; }
inline std::string sql_str(abieos::time_point_sec v)        { return v.utc_seconds ? quote(std::string(v)): null_value; }
inline std::string sql_str(abieos::block_timestamp v)       { return v.slot ?  quote(std::string(v)) : null_value; }
inline std::string sql_str(const abieos::checksum256& v)    { return quote(v.value == abieos::checksum256{}.value ? "" : std::string(v)); }
inline std::string sql_str(const abieos::public_key& v)     { return quote(public_key_to_string(v)); }
inline std::string sql_str(const abieos::bytes&)            { throw std::runtime_error("sql_str(bytes): not implemented"); }
inline std::string sql_str(transaction_status)              { throw std::runtime_error("sql_str(transaction_status): not implemented"); }
// clang-format on

template <typename T>
std::string bin_to_sql(abieos::input_buffer& bin) {
    return sql_str(abieos::read_raw<T>(bin));
}

template <>
inline std::string bin_to_sql<abieos::bytes>(abieos::input_buffer& bin) {
    abieos::input_buffer b;
    bin_to_native(b, bin);
    std::string result;
    abieos::hex(b.pos, b.end, back_inserter(result));
    return quote_bytea(result);
}

inline abieos::time_point sql_to_time_point(std::string s) {
    if (s.empty())
        return {};
    std::replace(s.begin(), s.end(), ' ', 'T');
    return abieos::string_to_time_point(s);
}

inline abieos::block_timestamp sql_to_block_timestamp(std::string s) {
    if (s.empty())
        return {};
    std::replace(s.begin(), s.end(), ' ', 'T');
    return abieos::block_timestamp{abieos::string_to_time_point(s)};
}

template <typename T>
void sql_to_bin(std::vector<char>& bin, const pqxx::field& f) {
    abieos::native_to_bin(bin, f.as<T>());
}

// clang-format off
template <>
void sql_to_bin<transaction_status>(std::vector<char>& bin, const pqxx::field& f) {
    if (false) {}
    else if (!strcmp("executed",  f.c_str())) abieos::native_to_bin(bin, (uint8_t)transaction_status::executed);
    else if (!strcmp("soft_fail", f.c_str())) abieos::native_to_bin(bin, (uint8_t)transaction_status::soft_fail);
    else if (!strcmp("hard_fail", f.c_str())) abieos::native_to_bin(bin, (uint8_t)transaction_status::hard_fail);
    else if (!strcmp("delayed",   f.c_str())) abieos::native_to_bin(bin, (uint8_t)transaction_status::delayed);
    else if (!strcmp("expired",   f.c_str())) abieos::native_to_bin(bin, (uint8_t)transaction_status::expired);
    else
        throw std::runtime_error("invalid value for transaction_status: " + f.as<std::string>());
}
// clang-format on

// clang-format off
template <> void sql_to_bin<uint8_t>                    (std::vector<char>& bin, const pqxx::field& f) { abieos::native_to_bin(bin, (uint8_t)f.as<uint16_t>()); }
template <> void sql_to_bin<int8_t>                     (std::vector<char>& bin, const pqxx::field& f) { abieos::native_to_bin(bin, (int8_t)f.as<int16_t>()); }
template <> void sql_to_bin<abieos::varuint32>          (std::vector<char>& bin, const pqxx::field& f) { abieos::push_varuint32(bin, f.as<uint32_t>()); }
template <> void sql_to_bin<abieos::varint32>           (std::vector<char>& bin, const pqxx::field& f) { abieos::push_varint32(bin, f.as<int32_t>()); }
template <> void sql_to_bin<abieos::int128>             (std::vector<char>& bin, const pqxx::field& f) { throw std::runtime_error("sql_to_bin<abieos::int128> not implemented"); }
template <> void sql_to_bin<abieos::uint128>            (std::vector<char>& bin, const pqxx::field& f) { throw std::runtime_error("sql_to_bin<abieos::uint128> not implemented"); }
template <> void sql_to_bin<abieos::float128>           (std::vector<char>& bin, const pqxx::field& f) { throw std::runtime_error("sql_to_bin<abieos::float128> not implemented"); }
template <> void sql_to_bin<abieos::name>               (std::vector<char>& bin, const pqxx::field& f) { abieos::native_to_bin(bin, abieos::name{f.c_str()}); }
template <> void sql_to_bin<abieos::time_point>         (std::vector<char>& bin, const pqxx::field& f) { abieos::native_to_bin(bin, sql_to_time_point(f.c_str())); }
template <> void sql_to_bin<abieos::time_point_sec>     (std::vector<char>& bin, const pqxx::field& f) { throw std::runtime_error("sql_to_bin<abieos::time_point_sec> not implemented"); }
template <> void sql_to_bin<abieos::block_timestamp>    (std::vector<char>& bin, const pqxx::field& f) { abieos::native_to_bin(bin, sql_to_block_timestamp(f.c_str())); }
template <> void sql_to_bin<abieos::checksum256>        (std::vector<char>& bin, const pqxx::field& f) { abieos::native_to_bin(bin, sql_to_checksum256(f.c_str())); }
template <> void sql_to_bin<abieos::public_key>         (std::vector<char>& bin, const pqxx::field& f) { throw std::runtime_error("sql_to_bin<abieos::public_key> not implemented"); }
template <> void sql_to_bin<abieos::bytes>              (std::vector<char>& bin, const pqxx::field& f) { abieos::native_to_bin(bin, sql_to_bytes(f.c_str())); }
// clang-format on

struct sql_type {
    const char* type                                               = "";
    std::string (*bin_to_sql)(abieos::input_buffer&)               = nullptr;
    void (*sql_to_bin)(std::vector<char>& bin, const pqxx::field&) = nullptr;
};

template <typename T>
constexpr sql_type make_sql_type_for(const char* name) {
    return sql_type{name, bin_to_sql<T>, sql_to_bin<T>};
}

template <typename T>
struct unknown_type {};

template <typename T>
inline constexpr unknown_type<T> sql_type_for;

// clang-format off
template<> inline constexpr sql_type sql_type_for<bool>                     = make_sql_type_for<bool>(                      "bool"                      );
template<> inline constexpr sql_type sql_type_for<uint8_t>                  = make_sql_type_for<uint8_t>(                   "smallint"                  );
template<> inline constexpr sql_type sql_type_for<int8_t>                   = make_sql_type_for<int8_t>(                    "smallint"                  );
template<> inline constexpr sql_type sql_type_for<uint16_t>                 = make_sql_type_for<uint16_t>(                  "integer"                   );
template<> inline constexpr sql_type sql_type_for<int16_t>                  = make_sql_type_for<int16_t>(                   "smallint"                  );
template<> inline constexpr sql_type sql_type_for<uint32_t>                 = make_sql_type_for<uint32_t>(                  "bigint"                    );
template<> inline constexpr sql_type sql_type_for<int32_t>                  = make_sql_type_for<int32_t>(                   "integer"                   );
template<> inline constexpr sql_type sql_type_for<uint64_t>                 = make_sql_type_for<uint64_t>(                  "decimal"                   );
template<> inline constexpr sql_type sql_type_for<int64_t>                  = make_sql_type_for<int64_t>(                   "bigint"                    );
template<> inline constexpr sql_type sql_type_for<abieos::varuint32>        = make_sql_type_for<abieos::varuint32>(         "bigint"                    );
template<> inline constexpr sql_type sql_type_for<abieos::varint32>         = make_sql_type_for<abieos::varint32>(          "integer"                   );
template<> inline constexpr sql_type sql_type_for<abieos::name>             = make_sql_type_for<abieos::name>(              "varchar(13)"               );
template<> inline constexpr sql_type sql_type_for<abieos::checksum256>      = make_sql_type_for<abieos::checksum256>(       "varchar(64)"               );
template<> inline constexpr sql_type sql_type_for<abieos::time_point>       = make_sql_type_for<abieos::time_point>(        "timestamp"                 );
template<> inline constexpr sql_type sql_type_for<abieos::block_timestamp>  = make_sql_type_for<abieos::block_timestamp>(   "timestamp"                 );
template<> inline constexpr sql_type sql_type_for<abieos::bytes>            = make_sql_type_for<abieos::bytes>(             "bytes"                     );
template<> inline constexpr sql_type sql_type_for<transaction_status>       = make_sql_type_for<transaction_status>(        "transaction_status_type"   );

inline const std::map<std::string_view, sql_type> abi_type_to_sql_type = {
    {"bool",                    sql_type_for<bool>},
    {"uint8",                   sql_type_for<uint8_t>},
    {"int8",                    sql_type_for<int8_t>},
    {"uint16",                  sql_type_for<uint16_t>},
    {"int16",                   sql_type_for<int16_t>},
    {"uint32",                  sql_type_for<uint32_t>},
    {"int32",                   sql_type_for<int32_t>},
    {"uint64",                  sql_type_for<uint64_t>},
    {"int64",                   sql_type_for<int64_t>},
    {"varuint32",               sql_type_for<abieos::varuint32>},
    {"varint32",                sql_type_for<abieos::varint32>},
    {"name",                    sql_type_for<abieos::name>},
    {"checksum256",             sql_type_for<abieos::checksum256>},
    {"time_point",              sql_type_for<abieos::time_point>},
    {"block_timestamp_type",    sql_type_for<abieos::block_timestamp>},
    {"bytes",                   sql_type_for<abieos::bytes>},
    {"transaction_status",      sql_type_for<transaction_status>},
};
// clang-format on

} // namespace sql_conversion

namespace query_config {

using namespace sql_conversion;

struct field {
    std::string name       = {};
    std::string short_name = {};
    std::string type       = {};
};

template <typename F>
constexpr void for_each_field(field*, F f) {
    f("name", abieos::member_ptr<&field::name>{});
    f("short_name", abieos::member_ptr<&field::short_name>{});
    f("type", abieos::member_ptr<&field::type>{});
};

struct key {
    std::string name           = {};
    std::string type           = {};
    std::string expression     = {};
    std::string arg_expression = {};
    bool        desc           = {};
};

template <typename F>
constexpr void for_each_field(key*, F f) {
    f("name", abieos::member_ptr<&key::name>{});
    f("type", abieos::member_ptr<&key::type>{});
    f("expression", abieos::member_ptr<&key::expression>{});
    f("arg_expression", abieos::member_ptr<&key::arg_expression>{});
    f("desc", abieos::member_ptr<&key::desc>{});
};

struct table {
    std::string           name         = {};
    std::vector<field>    fields       = {};
    std::vector<sql_type> types        = {};
    std::vector<key>      history_keys = {};
    std::vector<key>      keys         = {};

    std::map<std::string, field*> field_map = {};
};

template <typename F>
constexpr void for_each_field(table*, F f) {
    f("name", abieos::member_ptr<&table::name>{});
    f("fields", abieos::member_ptr<&table::fields>{});
    f("history_keys", abieos::member_ptr<&table::history_keys>{});
    f("keys", abieos::member_ptr<&table::keys>{});
};

struct query {
    abieos::name             wasm_name         = {};
    std::string              index             = {};
    std::string              function          = {};
    std::string              _table            = {};
    bool                     is_state          = {};
    bool                     limit_block_index = {};
    uint32_t                 max_results       = {};
    std::vector<key>         sort_keys         = {};
    std::vector<std::string> conditions        = {};

    std::vector<sql_type> types        = {};
    table*                result_table = {};
};

template <typename F>
constexpr void for_each_field(query*, F f) {
    f("wasm_name", abieos::member_ptr<&query::wasm_name>{});
    f("index", abieos::member_ptr<&query::index>{});
    f("function", abieos::member_ptr<&query::function>{});
    f("table", abieos::member_ptr<&query::_table>{});
    f("is_state", abieos::member_ptr<&query::is_state>{});
    f("limit_block_index", abieos::member_ptr<&query::limit_block_index>{});
    f("max_results", abieos::member_ptr<&query::max_results>{});
    f("sort_keys", abieos::member_ptr<&query::sort_keys>{});
    f("conditions", abieos::member_ptr<&query::conditions>{});
};

struct config {
    std::vector<table> tables  = {};
    std::vector<query> queries = {};

    std::map<std::string, table*>  table_map = {};
    std::map<abieos::name, query*> query_map = {};

    void prepare() {
        for (auto& table : tables) {
            table_map[table.name] = &table;
            for (auto& field : table.fields) {
                table.field_map[field.name] = &field;
                auto it                     = abi_type_to_sql_type.find(field.type);
                if (it == abi_type_to_sql_type.end())
                    throw std::runtime_error("table " + table.name + " field " + field.name + ": unknown type: " + field.type);
                table.types.push_back(it->second);
            }
        }

        for (auto& query : queries) {
            query_map[query.wasm_name] = &query;
            auto it                    = table_map.find(query._table);
            if (it == table_map.end())
                throw std::runtime_error("query " + (std::string)query.wasm_name + ": unknown table: " + query._table);
            query.result_table = it->second;
            for (auto& key : query.sort_keys) {
                std::string type = key.type;
                if (type.empty()) {
                    auto field_it = query.result_table->field_map.find(key.name);
                    if (field_it == query.result_table->field_map.end())
                        throw std::runtime_error("query " + (std::string)query.wasm_name + ": unknown field: " + key.name);
                    type = field_it->second->type;
                }

                auto type_it = abi_type_to_sql_type.find(type);
                if (type_it == abi_type_to_sql_type.end())
                    throw std::runtime_error("query " + (std::string)query.wasm_name + " key " + key.name + ": unknown type: " + type);
                query.types.push_back(type_it->second);
            }
        }
    }
};

template <typename F>
constexpr void for_each_field(config*, F f) {
    f("tables", abieos::member_ptr<&config::tables>{});
    f("queries", abieos::member_ptr<&config::queries>{});
};

} // namespace query_config
