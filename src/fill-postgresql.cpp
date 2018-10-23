// copyright defined in abieos/LICENSE.txt

#include "abieos.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/program_options.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <pqxx/pqxx>
#include <string>
#include <string_view>

using namespace abieos;
using namespace std::literals;

using std::cerr;
using std::enable_shared_from_this;
using std::exception;
using std::make_shared;
using std::make_unique;
using std::map;
using std::optional;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::string_view;
using std::to_string;
using std::unique_ptr;
using std::vector;

namespace asio      = boost::asio;
namespace bio       = boost::iostreams;
namespace bpo       = boost::program_options;
namespace websocket = boost::beast::websocket;

using asio::ip::tcp;
using boost::beast::flat_buffer;
using boost::system::error_code;

struct status {
    enum {
        executed  = 0, // succeed, no error handler executed
        soft_fail = 1, // objectively failed (not executed), error handler executed
        hard_fail = 2, // objectively failed and error handler objectively failed thus no state change
        delayed   = 3, // transaction delayed/deferred/scheduled for future execution
        expired   = 4, // transaction expired and storage space refuned to user
    };
};

struct sql_type {
    const char* type                                              = "";
    string (*bin_to_sql)(pqxx::connection&, bool, input_buffer&)  = nullptr;
    string (*native_to_sql)(pqxx::connection&, bool, const void*) = nullptr;
};

template <typename T>
struct unknown_type {};

inline constexpr bool is_known_type(sql_type) { return true; }

template <typename T>
inline constexpr bool is_known_type(unknown_type<T>) {
    return false;
}

template <typename T>
inline constexpr unknown_type<T> sql_type_for;

template <typename T>
string sql_str(pqxx::connection& c, bool bulk, const T& obj);

string sql_str(pqxx::connection& c, bool bulk, const std::string& s) {
    try {
        if (bulk) {
            string tmp = c.esc(s);
            string result;
            result.reserve(tmp.size());
            for (auto ch : tmp) {
                if (ch == '\t')
                    result += "\\t";
                else
                    result += ch;
            }
            return result;
        } else {
            return "'" + c.esc(s) + "'";
        }
    } catch (...) {
        string result;
        if (!bulk)
            result = "'";
        boost::algorithm::hex(s.begin(), s.end(), back_inserter(result));
        if (!bulk)
            result += "'";
        return result;
    }
}

// clang-format off
string sql_str(pqxx::connection&, bool bulk, bool v)               { if(bulk) return v ? "t" : "f"; return v ? "true" : "false";}
string sql_str(pqxx::connection&, bool bulk, varuint32 v)          { return string(v); }
string sql_str(pqxx::connection&, bool bulk, varint32 v)           { return string(v); }
string sql_str(pqxx::connection&, bool bulk, int128 v)             { return string(v); }
string sql_str(pqxx::connection&, bool bulk, uint128 v)            { return string(v); }
string sql_str(pqxx::connection&, bool bulk, float128 v)           { if(bulk) return "\\\\x" + string(v); return "'\\x" + string(v) + "'"; }
string sql_str(pqxx::connection&, bool bulk, name v)               { if(bulk) return string(v); return "'" + string(v) + "'"; }
string sql_str(pqxx::connection&, bool bulk, time_point v)         { if(bulk) return string(v); return "'" + string(v) + "'"; }
string sql_str(pqxx::connection&, bool bulk, time_point_sec v)     { if(bulk) return string(v); return "'" + string(v) + "'"; }
string sql_str(pqxx::connection&, bool bulk, block_timestamp v)    { if(bulk) return string(v); return "'" + string(v) + "'"; }
string sql_str(pqxx::connection&, bool bulk, checksum256 v)        { if(bulk) return string(v); return "'" + string(v) + "'"; }
// clang-format on

template <typename T>
string sql_str(pqxx::connection& c, bool bulk, const T& obj) {
    if constexpr (is_optional_v<T>) {
        if (obj) {
            return sql_str(c, bulk, *obj);
        } else if (is_string_v<typename T::value_type>) {
            if (bulk)
                return ""s;
            else
                return "''"s;
        } else {
            if (bulk)
                return "\\N"s;
            else
                return "null"s;
        }
    } else {
        return to_string(obj);
    }
}

template <typename T>
string bin_to_sql(pqxx::connection& c, bool bulk, input_buffer& bin) {
    if constexpr (is_optional_v<T>) {
        if (read_bin<bool>(bin)) {
            return bin_to_sql<typename T::value_type>(c, bulk, bin);
        } else if (is_string_v<typename T::value_type>) {
            if (bulk)
                return ""s;
            else
                return "''"s;
        } else {
            if (bulk)
                return "\\N"s;
            else
                return "null"s;
        }
    } else {
        return sql_str(c, bulk, read_bin<T>(bin));
    }
}

template <typename T>
string native_to_sql(pqxx::connection& c, bool bulk, const void* p) {
    return sql_str(c, bulk, *reinterpret_cast<const T*>(p));
}

template <typename T>
constexpr sql_type make_sql_type_for(const char* name) {
    return sql_type{name, bin_to_sql<T>, native_to_sql<T>};
}

template <>
string bin_to_sql<string>(pqxx::connection& c, bool bulk, input_buffer& bin) {
    return sql_str(c, bulk, read_string(bin));
}

template <>
string bin_to_sql<bytes>(pqxx::connection&, bool bulk, input_buffer& bin) {
    auto size = read_varuint32(bin);
    if (size > bin.end - bin.pos)
        throw error("invalid bytes size");
    string result;
    if (bulk)
        result = "\\\\x";
    else
        result = "'\\x";
    boost::algorithm::hex(bin.pos, bin.pos + size, back_inserter(result));
    bin.pos += size;
    if (!bulk)
        result += "'";
    return result;
}

template <>
string native_to_sql<bytes>(pqxx::connection&, bool bulk, const void* p) {
    auto&  obj = reinterpret_cast<const bytes*>(p)->data;
    string result;
    if (bulk)
        result = "\\\\x";
    else
        result = "'\\x";
    boost::algorithm::hex(obj.data(), obj.data() + obj.size(), back_inserter(result));
    if (!bulk)
        result += "'";
    return result;
}

template <>
string bin_to_sql<input_buffer>(pqxx::connection&, bool, input_buffer& bin) {
    throw error("bin_to_sql: input_buffer unsupported");
}

template <>
string native_to_sql<input_buffer>(pqxx::connection&, bool bulk, const void* p) {
    auto&  obj = *reinterpret_cast<const input_buffer*>(p);
    string result;
    if (bulk)
        result = "\\\\x";
    else
        result = "'\\x";
    boost::algorithm::hex(obj.pos, obj.end, back_inserter(result));
    if (!bulk)
        result += "'";
    return result;
}

// clang-format off
template<> inline constexpr sql_type sql_type_for<bool>            = make_sql_type_for<bool>(            "bool"        );
template<> inline constexpr sql_type sql_type_for<varuint32>       = make_sql_type_for<varuint32>(       "bigint"      );
template<> inline constexpr sql_type sql_type_for<varint32>        = make_sql_type_for<varint32>(        "integer"     );
template<> inline constexpr sql_type sql_type_for<uint8_t>         = make_sql_type_for<uint8_t>(         "smallint"    );
template<> inline constexpr sql_type sql_type_for<uint16_t>        = make_sql_type_for<uint16_t>(        "integer"     );
template<> inline constexpr sql_type sql_type_for<uint32_t>        = make_sql_type_for<uint32_t>(        "bigint"      );
template<> inline constexpr sql_type sql_type_for<uint64_t>        = make_sql_type_for<uint64_t>(        "decimal"     );
template<> inline constexpr sql_type sql_type_for<uint128>         = make_sql_type_for<uint128>(         "decimal"     );
template<> inline constexpr sql_type sql_type_for<int8_t>          = make_sql_type_for<int8_t>(          "smallint"    );
template<> inline constexpr sql_type sql_type_for<int16_t>         = make_sql_type_for<int16_t>(         "smallint"    );
template<> inline constexpr sql_type sql_type_for<int32_t>         = make_sql_type_for<int32_t>(         "integer"     );
template<> inline constexpr sql_type sql_type_for<int64_t>         = make_sql_type_for<int64_t>(         "bigint"      );
template<> inline constexpr sql_type sql_type_for<int128>          = make_sql_type_for<int128>(          "decimal"     );
template<> inline constexpr sql_type sql_type_for<double>          = make_sql_type_for<double>(          "float8"      );
template<> inline constexpr sql_type sql_type_for<float128>        = make_sql_type_for<float128>(        "bytea"       );
template<> inline constexpr sql_type sql_type_for<name>            = make_sql_type_for<name>(            "varchar(13)" );
template<> inline constexpr sql_type sql_type_for<string>          = make_sql_type_for<string>(          "varchar"     );
template<> inline constexpr sql_type sql_type_for<time_point>      = make_sql_type_for<time_point>(      "timestamp"   );
template<> inline constexpr sql_type sql_type_for<time_point_sec>  = make_sql_type_for<time_point_sec>(  "timestamp"   );
template<> inline constexpr sql_type sql_type_for<block_timestamp> = make_sql_type_for<block_timestamp>( "timestamp"   );
template<> inline constexpr sql_type sql_type_for<checksum256>     = make_sql_type_for<checksum256>(     "varchar(64)" );
template<> inline constexpr sql_type sql_type_for<bytes>           = make_sql_type_for<bytes>(           "bytea"       );
template<> inline constexpr sql_type sql_type_for<input_buffer>    = make_sql_type_for<input_buffer>(    "bytea"       );

template <typename T>
inline constexpr sql_type sql_type_for<std::optional<T>> = make_sql_type_for<std::optional<T>>(sql_type_for<T>.type);

const map<string, sql_type> abi_type_to_sql_type = {
    {"bool",                    sql_type_for<bool>},
    {"varuint",                 sql_type_for<varuint32>},
    {"varint",                  sql_type_for<varint32>},
    {"uint8",                   sql_type_for<uint8_t>},
    {"uint16",                  sql_type_for<uint16_t>},
    {"uint32",                  sql_type_for<uint32_t>},
    {"uint64",                  sql_type_for<uint64_t>},
    {"uint128",                 sql_type_for<uint128>},
    {"int8",                    sql_type_for<int8_t>},
    {"int16",                   sql_type_for<int16_t>},
    {"int32",                   sql_type_for<int32_t>},
    {"int64",                   sql_type_for<int64_t>},
    {"int128",                  sql_type_for<int128>},
    {"float64",                 sql_type_for<double>},
    {"float128",                sql_type_for<float128>},
    {"name",                    sql_type_for<name>},
    {"string",                  sql_type_for<string>},
    {"time_point",              sql_type_for<time_point>},
    {"time_point_sec",          sql_type_for<time_point_sec>},
    {"block_timestamp_type",    sql_type_for<block_timestamp>},
    {"checksum256",             sql_type_for<checksum256>},
    {"bytes",                   sql_type_for<bytes>},
};
// clang-format on

struct variant_header_zero {};

bool bin_to_native(variant_header_zero&, bin_to_native_state& state, bool) {
    if (read_varuint32(state.bin))
        throw std::runtime_error("unexpected variant value");
    return true;
}

bool json_to_native(variant_header_zero&, json_to_native_state&, event_type, bool) { return true; }

struct block_position {
    uint32_t    block_num = 0;
    checksum256 block_id  = {};
};

template <typename F>
constexpr void for_each_field(block_position*, F f) {
    f("block_num", member_ptr<&block_position::block_num>{});
    f("block_id", member_ptr<&block_position::block_id>{});
}

struct get_blocks_result_v0 {
    block_position           head;
    block_position           last_irreversible;
    optional<block_position> this_block;
    optional<input_buffer>   block;
    optional<input_buffer>   traces;
    optional<input_buffer>   deltas;
};

template <typename F>
constexpr void for_each_field(get_blocks_result_v0*, F f) {
    f("head", member_ptr<&get_blocks_result_v0::head>{});
    f("last_irreversible", member_ptr<&get_blocks_result_v0::last_irreversible>{});
    f("this_block", member_ptr<&get_blocks_result_v0::this_block>{});
    f("block", member_ptr<&get_blocks_result_v0::block>{});
    f("traces", member_ptr<&get_blocks_result_v0::traces>{});
    f("deltas", member_ptr<&get_blocks_result_v0::deltas>{});
}

struct row {
    bool         present;
    input_buffer data;
};

template <typename F>
constexpr void for_each_field(row*, F f) {
    f("present", member_ptr<&row::present>{});
    f("data", member_ptr<&row::data>{});
}

struct table_delta_v0 {
    string      name;
    vector<row> rows;
};

template <typename F>
constexpr void for_each_field(table_delta_v0*, F f) {
    f("name", member_ptr<&table_delta_v0::name>{});
    f("rows", member_ptr<&table_delta_v0::rows>{});
}

struct action_trace_authorization {
    name actor;
    name permission;
};

template <typename F>
constexpr void for_each_field(action_trace_authorization*, F f) {
    f("actor", member_ptr<&action_trace_authorization::actor>{});
    f("permission", member_ptr<&action_trace_authorization::permission>{});
}

struct action_trace_auth_sequence {
    name     account;
    uint64_t sequence;
};

template <typename F>
constexpr void for_each_field(action_trace_auth_sequence*, F f) {
    f("account", member_ptr<&action_trace_auth_sequence::account>{});
    f("sequence", member_ptr<&action_trace_auth_sequence::sequence>{});
}

struct action_trace_ram_delta {
    name    account;
    int64_t delta;
};

template <typename F>
constexpr void for_each_field(action_trace_ram_delta*, F f) {
    f("account", member_ptr<&action_trace_ram_delta::account>{});
    f("delta", member_ptr<&action_trace_ram_delta::delta>{});
}

struct recurse_action_trace;

struct action_trace {
    variant_header_zero                dummy;
    variant_header_zero                receipt_dummy;
    name                               receipt_receiver;
    checksum256                        receipt_act_digest;
    uint64_t                           receipt_global_sequence;
    uint64_t                           receipt_recv_sequence;
    vector<action_trace_auth_sequence> receipt_auth_sequence;
    varuint32                          receipt_code_sequence;
    varuint32                          receipt_abi_sequence;
    name                               account;
    name                               name;
    vector<action_trace_authorization> authorization;
    input_buffer                       data;
    bool                               context_free;
    int64_t                            elapsed;
    string                             console;
    vector<action_trace_ram_delta>     account_ram_deltas;
    optional<string>                   except;
    vector<recurse_action_trace>       inline_traces;
};

template <typename F>
constexpr void for_each_field(action_trace*, F f) {
    f("dummy", member_ptr<&action_trace::dummy>{});
    f("receipt_dummy", member_ptr<&action_trace::receipt_dummy>{});
    f("receipt_receiver", member_ptr<&action_trace::receipt_receiver>{});
    f("receipt_act_digest", member_ptr<&action_trace::receipt_act_digest>{});
    f("receipt_global_sequence", member_ptr<&action_trace::receipt_global_sequence>{});
    f("receipt_recv_sequence", member_ptr<&action_trace::receipt_recv_sequence>{});
    f("receipt_auth_sequence", member_ptr<&action_trace::receipt_auth_sequence>{});
    f("receipt_code_sequence", member_ptr<&action_trace::receipt_code_sequence>{});
    f("receipt_abi_sequence", member_ptr<&action_trace::receipt_abi_sequence>{});
    f("account", member_ptr<&action_trace::account>{});
    f("name", member_ptr<&action_trace::name>{});
    f("authorization", member_ptr<&action_trace::authorization>{});
    f("data", member_ptr<&action_trace::data>{});
    f("context_free", member_ptr<&action_trace::context_free>{});
    f("elapsed", member_ptr<&action_trace::elapsed>{});
    f("console", member_ptr<&action_trace::console>{});
    f("account_ram_deltas", member_ptr<&action_trace::account_ram_deltas>{});
    f("except", member_ptr<&action_trace::except>{});
    f("inline_traces", member_ptr<&action_trace::inline_traces>{});
}

struct recurse_action_trace : action_trace {};

bool bin_to_native(recurse_action_trace& obj, bin_to_native_state& state, bool start) {
    action_trace& o = obj;
    return bin_to_native(o, state, start);
}

bool json_to_native(recurse_action_trace& obj, json_to_native_state& state, event_type event, bool start) {
    action_trace& o = obj;
    return json_to_native(o, state, event, start);
}

struct recurse_transaction_trace;

struct transaction_trace {
    variant_header_zero               dummy;
    checksum256                       id;
    uint8_t                           status;
    uint32_t                          cpu_usage_us;
    varuint32                         net_usage_words;
    int64_t                           elapsed;
    uint64_t                          net_usage;
    bool                              scheduled;
    vector<action_trace>              action_traces;
    optional<string>                  except;
    vector<recurse_transaction_trace> failed_dtrx_trace; // !!!
};

template <typename F>
constexpr void for_each_field(transaction_trace*, F f) {
    f("dummy", member_ptr<&transaction_trace::dummy>{});
    f("transaction_id", member_ptr<&transaction_trace::id>{});
    f("status", member_ptr<&transaction_trace::status>{});
    f("cpu_usage_us", member_ptr<&transaction_trace::cpu_usage_us>{});
    f("net_usage_words", member_ptr<&transaction_trace::net_usage_words>{});
    f("elapsed", member_ptr<&transaction_trace::elapsed>{});
    f("net_usage", member_ptr<&transaction_trace::net_usage>{});
    f("scheduled", member_ptr<&transaction_trace::scheduled>{});
    f("action_traces", member_ptr<&transaction_trace::action_traces>{});
    f("except", member_ptr<&transaction_trace::except>{});
    f("failed_dtrx_trace", member_ptr<&transaction_trace::failed_dtrx_trace>{});
}

struct recurse_transaction_trace : transaction_trace {};

bool bin_to_native(recurse_transaction_trace& obj, bin_to_native_state& state, bool start) {
    transaction_trace& o = obj;
    return bin_to_native(o, state, start);
}

bool json_to_native(recurse_transaction_trace& obj, json_to_native_state& state, event_type event, bool start) {
    transaction_trace& o = obj;
    return json_to_native(o, state, event, start);
}

std::vector<char> zlib_decompress(input_buffer data) {
    std::vector<char>      out;
    bio::filtering_ostream decomp;
    decomp.push(bio::zlib_decompressor());
    decomp.push(bio::back_inserter(out));
    bio::write(decomp, data.pos, data.end - data.pos);
    bio::close(decomp);
    return out;
}

struct table_stream {
    pqxx::connection  c;
    pqxx::work        t;
    pqxx::tablewriter writer;

    table_stream(const string& name)
        : t(c)
        , writer(t, name) {}
};

struct session : enable_shared_from_this<session> {
    pqxx::connection                      sql_connection;
    tcp::resolver                         resolver;
    websocket::stream<tcp::socket>        stream;
    string                                host;
    string                                port;
    string                                schema;
    uint32_t                              stop_before   = 0;
    bool                                  drop_schema   = false;
    bool                                  create_schema = false;
    bool                                  received_abi  = false;
    uint32_t                              head          = 0;
    uint32_t                              irreversible  = 0;
    abi_def                               abi{};
    map<string, abi_type>                 abi_types;
    map<string, unique_ptr<table_stream>> table_streams;

    explicit session(
        asio::io_context& ioc, string host, string port, string schema, uint32_t stop_before, bool drop_schema, bool create_schema)
        : resolver(ioc)
        , stream(ioc)
        , host(move(host))
        , port(move(port))
        , schema(move(schema))
        , stop_before(stop_before)
        , drop_schema(drop_schema)
        , create_schema(create_schema) {

        stream.binary(true);
        if (drop_schema) {
            pqxx::work t(sql_connection);
            t.exec("drop schema if exists " + t.quote_name(this->schema) + " cascade");
            t.commit();
        }
    }

    void start() {
        resolver.async_resolve(host, port, [self = shared_from_this(), this](error_code ec, tcp::resolver::results_type results) {
            callback(ec, "resolve", [&] {
                asio::async_connect(
                    stream.next_layer(), results.begin(), results.end(), [self = shared_from_this(), this](error_code ec, auto&) {
                        callback(ec, "connect", [&] {
                            stream.async_handshake(host, "/", [self = shared_from_this(), this](error_code ec) {
                                callback(ec, "handshake", [&] { //
                                    start_read();
                                });
                            });
                        });
                    });
            });
        });
    }

    void start_read() {
        auto in_buffer = make_shared<flat_buffer>();
        stream.async_read(*in_buffer, [self = shared_from_this(), this, in_buffer](error_code ec, size_t) {
            callback(ec, "async_read", [&] {
                if (!received_abi)
                    receive_abi(in_buffer);
                else {
                    if (!receive_result(in_buffer))
                        return;
                }
                start_read();
            });
        });
    }

    void receive_abi(const shared_ptr<flat_buffer>& p) {
        auto data = p->data();
        if (!json_to_native(abi, string_view{(const char*)data.data(), data.size()}))
            throw runtime_error("abi parse error");
        check_abi_version(abi.version);
        abi_types    = create_contract(abi).abi_types;
        received_abi = true;

        if (create_schema)
            create_tables();
        {
            pqxx::work t(sql_connection);
            load_status(t);
            truncate(t, head);
            t.commit();
        }
        send_request();
    }

    template <typename T>
    void create_table(pqxx::work& t, const std::string& name, const std::string& pk, string fields) {
        for_each_field((T*)nullptr, [&](const char* field_name, auto member_ptr) {
            using type              = typename decltype(member_ptr)::member_type;
            constexpr auto sql_type = sql_type_for<type>;
            if constexpr (is_known_type(sql_type)) {
                fields += ", "s + t.quote_name(field_name) + " " + sql_type.type;
            }
        });

        string query = "create table " + t.quote_name(schema) + "." + t.quote_name(name) + "(" + fields + ", primary key (" + pk + "))";
        // printf("%s\n", query.c_str());
        t.exec(query);
    }

    void create_tables() {
        pqxx::work t(sql_connection);

        t.exec("create schema " + t.quote_name(schema));
        t.exec(
            "create table " + t.quote_name(schema) +
            R"(.received_blocks ("block_index" bigint, "block_id" varchar(64), primary key("block_index")))");
        t.exec("create table " + t.quote_name(schema) + R"(.status ("head" bigint, "irreversible" bigint))");
        t.exec("create unique index on " + t.quote_name(schema) + R"(.status ((true)))");
        t.exec("insert into " + t.quote_name(schema) + R"(.status values (0, 0))");

        // clang-format off
        create_table<action_trace_authorization>(   t, "action_trace_authorization",  "block_index, transaction_id, action_index, index",     "block_index bigint, transaction_id varchar(64), action_index integer, index integer, transaction_status smallint");
        create_table<action_trace_auth_sequence>(   t, "action_trace_auth_sequence",  "block_index, transaction_id, action_index, index",     "block_index bigint, transaction_id varchar(64), action_index integer, index integer, transaction_status smallint");
        create_table<action_trace_ram_delta>(       t, "action_trace_ram_delta",      "block_index, transaction_id, action_index, index",     "block_index bigint, transaction_id varchar(64), action_index integer, index integer, transaction_status smallint");
        create_table<action_trace>(                 t, "action_trace",                "block_index, transaction_id, action_index",            "block_index bigint, transaction_id varchar(64), action_index integer, parent_action_index integer, transaction_status smallint");
        create_table<transaction_trace>(            t, "transaction_trace",           "block_index, transaction_id",                          "block_index bigint, failed_dtrx_trace varchar(64)");
        // clang-format on

        for (auto& table : abi.tables) {
            auto& variant_type = get_type(table.type);
            if (!variant_type.filled_variant || variant_type.fields.size() != 1 || !variant_type.fields[0].type->filled_struct)
                throw std::runtime_error("don't know how to proccess " + variant_type.name);
            auto& type = *variant_type.fields[0].type;

            string fields = "block_index bigint, present bool";
            for (auto& f : type.fields) {
                if (f.type->filled_struct || f.type->filled_variant)
                    continue; // todo
                auto abi_type = f.type->name;
                if (abi_type.size() >= 1 && abi_type.back() == '?')
                    abi_type.resize(abi_type.size() - 1);
                auto it = abi_type_to_sql_type.find(abi_type);
                if (it == abi_type_to_sql_type.end())
                    throw std::runtime_error("don't know sql type for abi type: " + abi_type);
                fields += ", " + t.quote_name(f.name) + " " + it->second.type;
            }

            // todo: PK
            t.exec("create table " + t.quote_name(schema) + "." + table.type + "(" + fields + ")");
        }

        t.commit();
    } // create_tables()

    void load_status(pqxx::work& t) {
        auto r       = t.exec("select head, irreversible from " + t.quote_name(schema) + ".status")[0];
        head         = r[0].as<uint32_t>();
        irreversible = r[1].as<uint32_t>();
    }

    void write_status(pqxx::work& t, pqxx::pipeline& pipeline) {
        pipeline.insert(
            "update " + t.quote_name(schema) + ".status set head=" + to_string(head) + ", irreversible=" + to_string(irreversible));
    }

    void truncate(pqxx::work& t, uint32_t block) {
        auto trunc = [&](const std::string& name) {
            t.exec("delete from " + t.quote_name(schema) + "." + t.quote_name(name) + " where block_index >= " + to_string(block));
        };
        trunc("received_blocks");
        trunc("action_trace_authorization");
        trunc("action_trace_auth_sequence");
        trunc("action_trace_ram_delta");
        trunc("action_trace");
        trunc("transaction_trace");
        for (auto& table : abi.tables)
            trunc(table.type);
    }

    bool receive_result(const shared_ptr<flat_buffer>& p) {
        auto   data = p->data();
        string json;

        // !!! fill received_blocks
        // !!! use received_blocks during startup
        // !!! switch forks

        input_buffer bin{(const char*)data.data(), (const char*)data.data() + data.size()};
        check_variant(bin, get_type("result"), "get_blocks_result_v0");

        get_blocks_result_v0 result;
        if (!bin_to_native(result, bin))
            throw runtime_error("result conversion error");

        if (!result.this_block)
            return true;

        bool bulk = true;

        if (stop_before && result.this_block->block_num >= stop_before) {
            cerr << "block " << result.this_block->block_num << ": stop requested\n";
            close_streams();
            return false;
        }

        if (!(result.this_block->block_num % 200))
            close_streams();
        if (!(result.this_block->block_num % 100))
            cerr << "block " << result.this_block->block_num << "\n";

        pqxx::work     t(sql_connection);
        pqxx::pipeline pipeline(t);
        if (result.deltas)
            receive_deltas(result.this_block->block_num, *result.deltas, bulk, t, pipeline);
        if (result.traces)
            receive_traces(result.this_block->block_num, *result.traces, bulk, t, pipeline);

        head         = result.this_block->block_num;
        irreversible = result.last_irreversible.block_num;
        if (!bulk)
            write_status(t, pipeline);

        pipeline.complete();
        t.commit();
        return true;
    }

    void close_streams() {
        if (table_streams.empty())
            return;
        for (auto& [_, ts] : table_streams) {
            ts->writer.complete();
            ts->t.commit();
            ts.reset();
        }
        table_streams.clear();

        pqxx::work     t(sql_connection);
        pqxx::pipeline pipeline(t);
        write_status(t, pipeline);
        pipeline.complete();
        t.commit();
    }

    void receive_deltas(uint32_t block_num, input_buffer buf, bool bulk, pqxx::work& t, pqxx::pipeline& pipeline) {
        auto         data = zlib_decompress(buf);
        input_buffer bin{data.data(), data.data() + data.size()};

        auto     num     = read_varuint32(bin);
        unsigned numRows = 0;
        for (uint32_t i = 0; i < num; ++i) {
            check_variant(bin, get_type("table_delta"), "table_delta_v0");
            table_delta_v0 table_delta;
            if (!bin_to_native(table_delta, bin))
                throw runtime_error("table_delta conversion error (1)");

            auto& variant_type = get_type(table_delta.name);
            if (!variant_type.filled_variant || variant_type.fields.size() != 1 || !variant_type.fields[0].type->filled_struct)
                throw std::runtime_error("don't know how to proccess " + variant_type.name);
            auto& type = *variant_type.fields[0].type;

            for (auto& row : table_delta.rows) {
                check_variant(row.data, variant_type, 0u);
                string fields = "block_index, present";
                string values;
                if (bulk)
                    values = to_string(block_num) + "\t" + (row.present ? "t" : "f");
                else
                    values = to_string(block_num) + ", " + (row.present ? "true" : "false");
                for (auto& f : type.fields) {
                    if (f.type->filled_struct || f.type->filled_variant) {
                        string dummy;
                        if (!bin_to_json(row.data, f.type, dummy))
                            throw runtime_error("table_delta conversion error (2)");
                        continue;
                    }

                    auto abi_type    = f.type->name;
                    bool is_optional = false;
                    if (abi_type.size() >= 1 && abi_type.back() == '?') {
                        is_optional = true;
                        abi_type.resize(abi_type.size() - 1);
                    }
                    auto it = abi_type_to_sql_type.find(abi_type);
                    if (it == abi_type_to_sql_type.end())
                        throw std::runtime_error("don't know sql type for abi type: " + abi_type);
                    if (!it->second.bin_to_sql)
                        throw std::runtime_error("don't know how to process " + f.type->name);

                    fields += ", " + t.quote_name(f.name);
                    if (bulk) {
                        if (!is_optional || read_bin<bool>(row.data))
                            values += "\t" + it->second.bin_to_sql(sql_connection, bulk, row.data);
                        else
                            values += "\t\\N";
                    } else {
                        if (!is_optional || read_bin<bool>(row.data))
                            values += ", " + it->second.bin_to_sql(sql_connection, bulk, row.data);
                        else
                            values += ", null";
                    }
                }

                if (bulk) {
                    auto& ts = table_streams[table_delta.name];
                    if (!ts)
                        ts = make_unique<table_stream>(t.quote_name(schema) + "." + t.quote_name(table_delta.name));
                    // printf("...%s\n", values.c_str());
                    ts->writer.write_raw_line(values);
                } else {
                    string query = "insert into " + t.quote_name(schema) + "." + t.quote_name(table_delta.name) + "(" + fields +
                                   ") values (" + values + ")";
                    // printf("%s\n", query.c_str());
                    pipeline.insert(query);
                }
            }
            numRows += table_delta.rows.size();
        }
    } // receive_deltas

    void receive_traces(uint32_t block_num, input_buffer buf, bool bulk, pqxx::work& t, pqxx::pipeline& pipeline) {
        auto         data = zlib_decompress(buf);
        input_buffer bin{data.data(), data.data() + data.size()};
        auto         num = read_varuint32(bin);
        for (uint32_t i = 0; i < num; ++i) {
            transaction_trace trace;
            if (!bin_to_native(trace, bin))
                throw runtime_error("transaction_trace conversion error (1)");
            write_transaction_trace(block_num, trace, bulk, t, pipeline);
        }
    }

    void write_transaction_trace(uint32_t block_num, transaction_trace& ttrace, bool bulk, pqxx::work& t, pqxx::pipeline& pipeline) {
        string fields = "block_index, failed_dtrx_trace";
        string values;
        if (bulk) {
            if (ttrace.failed_dtrx_trace.empty())
                values = to_string(block_num) + "\t";
            else
                values = to_string(block_num) + "\t" + string(ttrace.failed_dtrx_trace[0].id);
        } else {
            if (ttrace.failed_dtrx_trace.empty())
                values = to_string(block_num) + ", ''";
            else
                values = to_string(block_num) + ", '" + string(ttrace.failed_dtrx_trace[0].id) + "'";
        }

        write("transaction_trace", ttrace, fields, values, bulk, t, pipeline);
        int32_t num_actions = 0;
        for (auto& atrace : ttrace.action_traces)
            write_action_trace(block_num, ttrace, num_actions, 0, atrace, bulk, t, pipeline);
        if (!ttrace.failed_dtrx_trace.empty()) {
            auto& child = ttrace.failed_dtrx_trace[0];
            if (child.status == status::executed) {
                // child didn't execute correctly; fix status
                if (ttrace.status)
                    child.status = status::hard_fail;
                else
                    child.status = status::soft_fail;
            }
            write_transaction_trace(block_num, child, bulk, t, pipeline);
        }
    }

    void write_action_trace(
        uint32_t block_num, transaction_trace& ttrace, int32_t& num_actions, int32_t parent_action_index, action_trace& atrace, bool bulk,
        pqxx::work& t, pqxx::pipeline& pipeline) {

        const auto action_index = ++num_actions;
        string     fields       = "block_index, transaction_id, action_index, parent_action_index, transaction_status";
        string     values;
        if (bulk)
            values = to_string(block_num) + "\t" + (string)ttrace.id + "\t" + to_string(action_index) + "\t" +
                     to_string(parent_action_index) + "\t" + to_string(ttrace.status);
        else
            values = to_string(block_num) + ", '" + (string)ttrace.id + "', " + to_string(action_index) + ", " +
                     to_string(parent_action_index) + ", " + to_string(ttrace.status);

        write("action_trace", atrace, fields, values, bulk, t, pipeline);
        for (auto& child : atrace.inline_traces)
            write_action_trace(block_num, ttrace, num_actions, action_index, child, bulk, t, pipeline);

        write_action_trace_subtable("action_trace_authorization", block_num, ttrace, action_index, atrace.authorization, bulk, t, pipeline);
        write_action_trace_subtable(
            "action_trace_auth_sequence", block_num, ttrace, action_index, atrace.receipt_auth_sequence, bulk, t, pipeline);
        write_action_trace_subtable(
            "action_trace_ram_delta", block_num, ttrace, action_index, atrace.account_ram_deltas, bulk, t, pipeline);
    }

    template <typename T>
    void write_action_trace_subtable(
        const std::string& name, uint32_t block_num, transaction_trace& ttrace, int32_t action_index, T& objects, bool bulk, pqxx::work& t,
        pqxx::pipeline& pipeline) {

        int32_t num = 0;
        for (auto& obj : objects)
            write_action_trace_subtable(name, block_num, ttrace, action_index, num, obj, bulk, t, pipeline);
    }

    template <typename T>
    void write_action_trace_subtable(
        const std::string& name, uint32_t block_num, transaction_trace& ttrace, int32_t action_index, int32_t& num, T& obj, bool bulk,
        pqxx::work& t, pqxx::pipeline& pipeline) {

        string fields = "block_index, transaction_id, action_index, index, transaction_status";
        string values;
        if (bulk)
            values = to_string(block_num) + "\t" + (string)ttrace.id + "\t" + to_string(action_index) + "\t" + to_string(++num) + "\t" +
                     to_string(ttrace.status);
        else
            values = to_string(block_num) + ", '" + (string)ttrace.id + "', " + to_string(action_index) + ", " + to_string(++num) + "," +
                     to_string(ttrace.status);

        write(name, obj, fields, values, bulk, t, pipeline);
    }

    template <typename T>
    void write( //
        const std::string& name, T& obj, std::string fields, std::string values, bool bulk, pqxx::work& t, pqxx::pipeline& pipeline) {

        for_each_field((T*)nullptr, [&](const char* field_name, auto member_ptr) {
            using type              = typename decltype(member_ptr)::member_type;
            constexpr auto sql_type = sql_type_for<type>;
            if constexpr (is_known_type(sql_type)) {
                fields += ", " + t.quote_name(field_name);
                if (bulk)
                    values += "\t";
                else
                    values += ", ";
                values += sql_type.native_to_sql(sql_connection, bulk, &member_from_void(member_ptr, &obj));
            }
        });

        if (bulk) {
            auto& ts = table_streams[name];
            if (!ts)
                ts = make_unique<table_stream>(t.quote_name(schema) + "." + t.quote_name(name));
            // printf("...%s\n", values.c_str());
            ts->writer.write_raw_line(values);
        } else {
            string query = "insert into " + t.quote_name(schema) + "." + t.quote_name(name) + "(" + fields + ") values (" + values + ")";
            // printf("%s\n", query.c_str());
            pipeline.insert(query);
        }
    }

    void send_request() {
        send(jvalue{jarray{{"get_blocks_request_v0"s},
                           {jobject{
                               {{"start_block_num"s}, {to_string(head)}},
                               {{"end_block_num"s}, {"4294967295"s}},
                               {{"max_messages_in_flight"s}, {"4294967295"s}},
                               {{"have_positions"s}, {jarray{}}},
                               {{"irreversible_only"s}, {false}},
                               {{"fetch_block"s}, {false}},
                               {{"fetch_traces"s}, {true}},
                               {{"fetch_deltas"s}, {true}},
                           }}}});
    }

    const abi_type& get_type(const string& name) {
        auto it = abi_types.find(name);
        if (it == abi_types.end())
            throw runtime_error("unknown type "s + name);
        return it->second;
    }

    void send(const jvalue& value) {
        auto bin = make_shared<vector<char>>();
        if (!json_to_bin(*bin, &get_type("request"), value))
            throw runtime_error("failed to convert during send");

        stream.async_write(
            asio::buffer(*bin), [self = shared_from_this(), bin, this](error_code ec, size_t) { callback(ec, "async_write", [&] {}); });
    }

    void check_variant(input_buffer& bin, const abi_type& type, uint32_t expected) {
        auto index = read_varuint32(bin);
        if (!type.filled_variant)
            throw runtime_error(type.name + " is not a variant"s);
        if (index >= type.fields.size())
            throw runtime_error("expected "s + type.fields[expected].name + " got " + to_string(index));
        if (index != expected)
            throw runtime_error("expected "s + type.fields[expected].name + " got " + type.fields[index].name);
    }

    void check_variant(input_buffer& bin, const abi_type& type, const char* expected) {
        auto index = read_varuint32(bin);
        if (!type.filled_variant)
            throw runtime_error(type.name + " is not a variant"s);
        if (index >= type.fields.size())
            throw runtime_error("expected "s + expected + " got " + to_string(index));
        if (type.fields[index].name != expected)
            throw runtime_error("expected "s + expected + " got " + type.fields[index].name);
    }

    template <typename F>
    void catch_and_close(F f) {
        try {
            f();
        } catch (const exception& e) {
            cerr << "error: " << e.what() << "\n";
            close();
        } catch (...) {
            cerr << "error: unknown exception\n";
            close();
        }
    }

    template <typename F>
    void callback(error_code ec, const char* what, F f) {
        if (ec)
            return on_fail(ec, what);
        catch_and_close(f);
    }

    void on_fail(error_code ec, const char* what) {
        try {
            cerr << what << ": " << ec.message() << "\n";
            close();
        } catch (...) {
            cerr << "error: exception while closing\n";
        }
    }

    void close() { stream.next_layer().close(); }
}; // session

int main(int argc, char** argv) {
    try {
        bpo::options_description desc{"Options"};
        auto                     op = desc.add_options();
        op("help,h", "Help screen");
        op("host,H", bpo::value<string>()->default_value("localhost"), "Host to connect to (nodeos)");
        op("port,p", bpo::value<string>()->default_value("8080"), "Port to connect to (nodeos state-history plugin)");
        op("schema,s", bpo::value<string>()->default_value("chain"), "Database schema");
        op("stop,x", bpo::value<uint32_t>(), "Stop before block [arg]");
        op("drop,d", "Drop (delete) schema and tables");
        op("create,c", "Create schema and tables");

        bpo::variables_map vm;
        bpo::store(bpo::parse_command_line(argc, argv, desc), vm);
        bpo::notify(vm);

        if (vm.count("help"))
            std::cout << desc << '\n';
        else {
            asio::io_context ioc;
            auto             s = make_shared<session>(
                ioc, vm["host"].as<string>(), vm["port"].as<string>(), vm["schema"].as<string>(),
                vm.count("stop") ? vm["stop"].as<uint32_t>() : 0, vm.count("drop"), vm.count("create"));
            s->start();
            ioc.run();
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
