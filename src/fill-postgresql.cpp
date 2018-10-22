// copyright defined in abieos/LICENSE.txt

#include "abieos.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
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
namespace websocket = boost::beast::websocket;

using asio::ip::tcp;
using boost::beast::flat_buffer;
using boost::system::error_code;

struct sql_type {
    const char* type                                        = "";
    string (*bin_to_sql)(pqxx::connection&, input_buffer&)  = nullptr;
    string (*native_to_sql)(pqxx::connection&, const void*) = nullptr;
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
string sql_str(pqxx::connection& c, const T& obj);

string sql_str(pqxx::connection& c, const std::string& s) {
    try {
        return "'" + c.esc(s) + "'";
    } catch (...) {
        string result = "'";
        boost::algorithm::hex(s.begin(), s.end(), back_inserter(result));
        result += "'";
        return result;
    }
}

// clang-format off
string sql_str(pqxx::connection&, bool v)               { return v ? "true" : "false"; }
string sql_str(pqxx::connection&, varuint32 v)          { return string(v); }
string sql_str(pqxx::connection&, varint32 v)           { return string(v); }
string sql_str(pqxx::connection&, int128 v)             { return string(v); }
string sql_str(pqxx::connection&, uint128 v)            { return string(v); }
string sql_str(pqxx::connection&, float128 v)           { return "'\\x" + string(v) + "'"; }
string sql_str(pqxx::connection&, name v)               { return "'" + string(v) + "'"; }
string sql_str(pqxx::connection&, time_point v)         { return "'" + string(v) + "'"; }
string sql_str(pqxx::connection&, time_point_sec v)     { return "'" + string(v) + "'"; }
string sql_str(pqxx::connection&, block_timestamp v)    { return "'" + string(v) + "'"; }
string sql_str(pqxx::connection&, checksum256 v)        { return "'" + string(v) + "'"; }
// clang-format on

template <typename T>
string sql_str(pqxx::connection& c, const T& obj) {
    if constexpr (is_optional_v<T>) {
        if (obj)
            return sql_str(c, *obj);
        else if (is_string_v<typename T::value_type>)
            return "''"s;
        else
            return "null"s;
    } else {
        return to_string(obj);
    }
}

template <typename T>
string bin_to_sql(pqxx::connection& c, input_buffer& bin) {
    if constexpr (is_optional_v<T>) {
        if (read_bin<bool>(bin))
            return bin_to_sql<typename T::value_type>(c, bin);
        else if (is_string_v<typename T::value_type>)
            return "''"s;
        else
            return "null"s;
    } else {
        return sql_str(c, read_bin<T>(bin));
    }
}

template <typename T>
string native_to_sql(pqxx::connection& c, const void* p) {
    return sql_str(c, *reinterpret_cast<const T*>(p));
}

template <typename T>
constexpr sql_type make_sql_type_for(const char* name) {
    return sql_type{name, bin_to_sql<T>, native_to_sql<T>};
}

template <>
string bin_to_sql<string>(pqxx::connection& c, input_buffer& bin) {
    return sql_str(c, read_string(bin));
}

template <>
string bin_to_sql<bytes>(pqxx::connection&, input_buffer& bin) {
    auto size = read_varuint32(bin);
    if (size > bin.end - bin.pos)
        throw error("invalid bytes size");
    string result = "'\\x";
    boost::algorithm::hex(bin.pos, bin.pos + size, back_inserter(result));
    bin.pos += size;
    result += "'";
    return result;
}

template <>
string native_to_sql<bytes>(pqxx::connection&, const void* p) {
    auto&  obj    = reinterpret_cast<const bytes*>(p)->data;
    string result = "'\\x";
    boost::algorithm::hex(obj.data(), obj.data() + obj.size(), back_inserter(result));
    result += "'";
    return result;
}

template <>
string bin_to_sql<input_buffer>(pqxx::connection&, input_buffer& bin) {
    throw error("bin_to_sql: input_buffer unsupported");
}

template <>
string native_to_sql<input_buffer>(pqxx::connection&, const void* p) {
    auto&  obj    = *reinterpret_cast<const input_buffer*>(p);
    string result = "'\\x";
    boost::algorithm::hex(obj.pos, obj.end, back_inserter(result));
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

struct session : enable_shared_from_this<session> {
    pqxx::connection               sql_connection;
    tcp::resolver                  resolver;
    websocket::stream<tcp::socket> stream;
    string                         host;
    string                         port;
    string                         schema;
    bool                           received_abi = false;
    abi_def                        abi{};
    map<string, abi_type>          abi_types;

    explicit session(asio::io_context& ioc, string host, string port, string schema)
        : resolver(ioc)
        , stream(ioc)
        , host(move(host))
        , port(move(port))
        , schema(move(schema)) {
        stream.binary(true);
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
                else
                    receive_result(in_buffer);
                start_read();
            });
        });
    }

    void receive_abi(const shared_ptr<flat_buffer>& p) {
        printf("received abi\n");
        auto data = p->data();
        if (!json_to_native(abi, string_view{(const char*)data.data(), data.size()}))
            throw runtime_error("abi parse error");
        check_abi_version(abi.version);
        abi_types    = create_contract(abi).abi_types;
        received_abi = true;
        create_tables();
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

        string query = "create table " + schema + "." + t.quote_name(name) + "(" + fields + ", primary key (" + pk + "))";
        printf("%s\n", query.c_str());
        t.exec(query);
    }

    void create_tables() {
        pqxx::work t(sql_connection);

        t.exec("drop schema if exists " + schema + " cascade");
        t.exec("create schema " + schema);
        t.exec(
            "create table " + schema +
            R"(.received_blocks ("block_index" bigint, "block_id" varchar(64), primary key("block_index")))");
        t.exec("create table " + schema + R"(.status ("head" bigint, "irreversible" bigint))");
        t.exec("create unique index on " + schema + R"(.status ((true)))");
        t.exec("insert into " + schema + R"(.status values (0, 0))");

        // clang-format off
        create_table<action_trace_authorization>(   t, "action_trace_authorization",  "block_index, transaction_id, action_index, index",     "block_index bigint, transaction_id varchar(64), action_index integer, index integer, transaction_status smallint");
        create_table<action_trace_auth_sequence>(   t, "action_trace_auth_sequence",  "block_index, transaction_id, action_index, index",     "block_index bigint, transaction_id varchar(64), action_index integer, index integer, transaction_status smallint");
        create_table<action_trace_ram_delta>(       t, "action_trace_ram_delta",      "block_index, transaction_id, action_index, index",     "block_index bigint, transaction_id varchar(64), action_index integer, index integer, transaction_status smallint");
        create_table<action_trace>(                 t, "action_trace",                "block_index, transaction_id, action_index",            "block_index bigint, transaction_id varchar(64), action_index integer, parent_action_index integer, transaction_status smallint");
        create_table<transaction_trace>(            t, "transaction_trace",           "block_index, transaction_id",                          "block_index bigint");
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
            t.exec("create table " + schema + "." + table.type + "(" + fields + ")");
        }

        t.commit();
    }

    void receive_result(const shared_ptr<flat_buffer>& p) {
        auto   data = p->data();
        string json;

        input_buffer bin{(const char*)data.data(), (const char*)data.data() + data.size()};
        check_variant(bin, get_type("result"), "get_blocks_result_v0");

        get_blocks_result_v0 result;
        if (!bin_to_native(result, bin))
            throw runtime_error("result conversion error");

        if (!result.this_block)
            return;
        if (!(result.this_block->block_num % 100))
            printf("block %d\n", result.this_block->block_num);
        if (result.this_block->block_num == 1'000)
            throw std::runtime_error("stop");

        pqxx::work     t(sql_connection);
        pqxx::pipeline pipeline(t);
        if (result.deltas)
            receive_deltas(result.this_block->block_num, *result.deltas, t, pipeline);
        if (result.traces)
            receive_traces(result.this_block->block_num, *result.traces, t, pipeline);
        pipeline.complete();
        t.commit();
    }

    void receive_deltas(uint32_t block_num, input_buffer buf, pqxx::work& t, pqxx::pipeline& pipeline) {
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
                string values = to_string(block_num) + ", " + (row.present ? "true" : "false");
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
                    if (!is_optional || read_bin<bool>(row.data))
                        values += ", " + it->second.bin_to_sql(sql_connection, row.data);
                    else
                        values += ", null";
                }

                pipeline.insert("insert into " + schema + "." + table_delta.name + "(" + fields + ") values (" + values + ")");
            }
            numRows += table_delta.rows.size();
        }
    } // receive_deltas

    void receive_traces(uint32_t block_num, input_buffer buf, pqxx::work& t, pqxx::pipeline& pipeline) {
        auto         data = zlib_decompress(buf);
        input_buffer bin{data.data(), data.data() + data.size()};
        auto         num = read_varuint32(bin);
        for (uint32_t i = 0; i < num; ++i) {
            transaction_trace trace;
            if (!bin_to_native(trace, bin))
                throw runtime_error("transaction_trace conversion error (1)");
            write_transaction_trace(block_num, trace, t, pipeline);
        }
    }

    void write_transaction_trace(uint32_t block_num, transaction_trace& ttrace, pqxx::work& t, pqxx::pipeline& pipeline) {
        write("transaction_trace", ttrace, "block_index", to_string(block_num), t, pipeline);
        int32_t num_actions = 0;
        for (auto& atrace : ttrace.action_traces)
            write_action_trace(block_num, ttrace, num_actions, 0, atrace, t, pipeline);
    }

    void write_action_trace(
        uint32_t block_num, transaction_trace& ttrace, int32_t& num_actions, int32_t parent_action_index, action_trace& atrace,
        pqxx::work& t, pqxx::pipeline& pipeline) {

        const auto action_index = ++num_actions;
        write(
            "action_trace", atrace, "block_index, transaction_id, action_index, parent_action_index, transaction_status",
            to_string(block_num) + ", '" + (string)ttrace.id + "', " + to_string(action_index) + ", " + to_string(parent_action_index) +
                "," + to_string(ttrace.status),
            t, pipeline);
        for (auto& child : atrace.inline_traces)
            write_action_trace(block_num, ttrace, num_actions, action_index, child, t, pipeline);

        write_action_trace_subtable("action_trace_authorization", block_num, ttrace, action_index, atrace.authorization, t, pipeline);
        write_action_trace_subtable(
            "action_trace_auth_sequence", block_num, ttrace, action_index, atrace.receipt_auth_sequence, t, pipeline);
        write_action_trace_subtable("action_trace_ram_delta", block_num, ttrace, action_index, atrace.account_ram_deltas, t, pipeline);
    }

    template <typename T>
    void write_action_trace_subtable(
        const std::string& name, uint32_t block_num, transaction_trace& ttrace, int32_t action_index, T& objects, pqxx::work& t,
        pqxx::pipeline& pipeline) {

        int32_t num = 0;
        for (auto& obj : objects)
            write_action_trace_subtable(name, block_num, ttrace, action_index, num, obj, t, pipeline);
    }

    template <typename T>
    void write_action_trace_subtable(
        const std::string& name, uint32_t block_num, transaction_trace& ttrace, int32_t action_index, int32_t& num, T& obj, pqxx::work& t,
        pqxx::pipeline& pipeline) {

        write(
            name, obj, "block_index, transaction_id, action_index, index, transaction_status",
            to_string(block_num) + ", '" + (string)ttrace.id + "', " + to_string(action_index) + ", " + to_string(++num) + "," +
                to_string(ttrace.status),
            t, pipeline);
    }

    template <typename T>
    void write(const std::string& name, T& obj, std::string fields, std::string values, pqxx::work& t, pqxx::pipeline& pipeline) {
        for_each_field((T*)nullptr, [&](const char* field_name, auto member_ptr) {
            using type              = typename decltype(member_ptr)::member_type;
            constexpr auto sql_type = sql_type_for<type>;
            if constexpr (is_known_type(sql_type)) {
                fields += ", " + t.quote_name(field_name);
                values += ", " + sql_type.native_to_sql(sql_connection, &member_from_void(member_ptr, &obj));
            }
        });

        string query = "insert into " + schema + "." + t.quote_name(name) + "(" + fields + ") values (" + values + ")";
        // printf("%s\n", query.c_str());
        pipeline.insert(query);
    }

    void send_request() {
        send(jvalue{jarray{{"get_blocks_request_v0"s},
                           {jobject{
                               {{"start_block_num"s}, {"0"s}},
                               {{"end_block_num"s}, {"4294967295"s}},
                               {{"max_messages_in_flight"s}, {"4294967295"s}},
                               {{"have_positions"s}, {jarray{}}},
                               {{"irreversible_only"s}, {false}},
                               {{"fetch_block"s}, {false}},
                               {{"fetch_traces"s}, {true}},
                               {{"fetch_deltas"s}, {false}},
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
    if (argc != 4) {
        cerr << "Usage: websocket-client-async <host> <port> <schema>\n"
             << "Example:\n"
             << "    websocket-client-async localhost 8080\n";
        return EXIT_FAILURE;
    }
    auto host   = argv[1];
    auto port   = argv[2];
    auto schema = argv[3];

    asio::io_context ioc;
    make_shared<session>(ioc, host, port, schema)->start();
    ioc.run();

    return EXIT_SUCCESS;
}
