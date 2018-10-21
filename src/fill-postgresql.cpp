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
    const char* type                    = "";
    string (*bin_to_sql)(input_buffer&) = nullptr;
};

string bin_to_sql_bool(input_buffer& bin) {
    if (read_bin<bool>(bin))
        return "true";
    else
        return "false";
}

template <typename T>
string bin_to_sql_int(input_buffer& bin) {
    return to_string(read_bin<T>(bin));
}

string bin_to_sql_bytes(input_buffer& bin) {
    auto size = read_varuint32(bin);
    if (size > bin.end - bin.pos)
        throw error("invalid bytes size");
    string result = "'\\x";
    boost::algorithm::hex(bin.pos, bin.pos + size, back_inserter(result));
    bin.pos += size;
    result += "'";
    return result;
}

const map<string, sql_type> sql_types = {
    {"bool", {"bool", bin_to_sql_bool}},
    {"varuint", {"bigint", [](auto& bin) { return to_string(read_varuint32(bin)); }}},
    {"varint", {"integer", [](auto& bin) { return to_string(read_varint32(bin)); }}},
    {"uint8", {"smallint", bin_to_sql_int<uint8_t>}},
    {"uint16", {"integer", bin_to_sql_int<uint16_t>}},
    {"uint32", {"bigint", bin_to_sql_int<uint32_t>}},
    {"uint64", {"decimal", bin_to_sql_int<uint64_t>}},
    {"uint128", {"decimal", [](auto& bin) { return (string)read_bin<uint128>(bin); }}},
    {"int8", {"smallint", bin_to_sql_int<int8_t>}},
    {"int16", {"smallint", bin_to_sql_int<int16_t>}},
    {"int32", {"integer", bin_to_sql_int<int32_t>}},
    {"int64", {"bigint", bin_to_sql_int<int64_t>}},
    {"int128", {"decimal", [](auto& bin) { return (string)read_bin<int128>(bin); }}},
    {"float64", {"float8", bin_to_sql_int<double>}},
    {"float128", {"bytea", [](auto& bin) { return "'\\x" + string(read_bin<float128>(bin)) + "'"; }}},
    {"name", {"varchar(13)", [](auto& bin) { return "'" + name_to_string(read_bin<uint64_t>(bin)) + "'"; }}},
    {"time_point", {"timestamp", [](auto& bin) { return "'" + string(read_bin<time_point>(bin)) + "'"; }}},
    {"time_point_sec", {"timestamp", [](auto& bin) { return "'" + string(read_bin<time_point_sec>(bin)) + "'"; }}},
    {"block_timestamp_type", {"timestamp", [](auto& bin) { return "'" + string(read_bin<block_timestamp>(bin)) + "'"; }}},
    {"checksum256", {"varchar(64)", [](auto& bin) { return "'" + string(read_bin<checksum256>(bin)) + "'"; }}},
    {"bytes", {"bytea", bin_to_sql_bytes}},
};

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

struct permission_level {
    name actor;
    name permission;
};

template <typename F>
constexpr void for_each_field(permission_level*, F f) {
    f("actor", member_ptr<&permission_level::actor>{});
    f("permission", member_ptr<&permission_level::permission>{});
}

struct account_auth_sequence {
    name     account;
    uint64_t sequence;
};

template <typename F>
constexpr void for_each_field(account_auth_sequence*, F f) {
    f("account", member_ptr<&account_auth_sequence::account>{});
    f("sequence", member_ptr<&account_auth_sequence::sequence>{});
}

struct action_receipt {
    variant_header_zero           dummy;
    name                          receiver;
    checksum256                   act_digest;
    uint64_t                      global_sequence;
    uint64_t                      recv_sequence;
    vector<account_auth_sequence> auth_sequence;
    varuint32                     code_sequence;
    varuint32                     abi_sequence;
};

template <typename F>
constexpr void for_each_field(action_receipt*, F f) {
    f("dummy", member_ptr<&action_receipt::dummy>{});
    f("receiver", member_ptr<&action_receipt::receiver>{});
    f("act_digest", member_ptr<&action_receipt::act_digest>{});
    f("global_sequence", member_ptr<&action_receipt::global_sequence>{});
    f("recv_sequence", member_ptr<&action_receipt::recv_sequence>{});
    f("auth_sequence", member_ptr<&action_receipt::auth_sequence>{});
    f("code_sequence", member_ptr<&action_receipt::code_sequence>{});
    f("abi_sequence", member_ptr<&action_receipt::abi_sequence>{});
}

struct action {
    name                     account;
    name                     name;
    vector<permission_level> authorization;
    input_buffer             data;
};

template <typename F>
constexpr void for_each_field(action*, F f) {
    f("account", member_ptr<&action::account>{});
    f("name", member_ptr<&action::name>{});
    f("authorization", member_ptr<&action::authorization>{});
    f("data", member_ptr<&action::data>{});
}

struct account_delta {
    name    account;
    int64_t delta;
};

template <typename F>
constexpr void for_each_field(account_delta*, F f) {
    f("account", member_ptr<&account_delta::account>{});
    f("delta", member_ptr<&account_delta::delta>{});
}

struct recurse_action_trace;

struct action_trace {
    variant_header_zero          dummy;
    action_receipt               receipt;
    action                       act;
    bool                         context_free;
    int64_t                      elapsed;
    string                       console;
    vector<account_delta>        account_ram_deltas;
    optional<string>             except;
    vector<recurse_action_trace> inline_traces;
};

template <typename F>
constexpr void for_each_field(action_trace*, F f) {
    f("dummy", member_ptr<&action_trace::dummy>{});
    f("receipt", member_ptr<&action_trace::receipt>{});
    f("act", member_ptr<&action_trace::act>{});
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
    vector<recurse_transaction_trace> failed_dtrx_trace;
};

template <typename F>
constexpr void for_each_field(transaction_trace*, F f) {
    f("dummy", member_ptr<&transaction_trace::dummy>{});
    f("id", member_ptr<&transaction_trace::id>{});
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

        for (auto& table : abi.tables) {
            auto& variant_type = get_type(table.type);
            if (!variant_type.filled_variant || variant_type.fields.size() != 1 || !variant_type.fields[0].type->filled_struct)
                throw std::runtime_error("don't know how to proccess " + variant_type.name);
            auto& type = *variant_type.fields[0].type;

            string fields;
            for (auto& f : type.fields) {
                if (f.type->filled_struct || f.type->filled_variant)
                    continue; // todo
                auto abi_type = f.type->name;
                if (abi_type.size() >= 1 && abi_type.back() == '?')
                    abi_type.resize(abi_type.size() - 1);
                auto it = sql_types.find(abi_type);
                if (it == sql_types.end())
                    throw std::runtime_error("don't know sql type for abi type: " + abi_type);
                if (!fields.empty())
                    fields += ", ";
                fields += t.quote_name(f.name) + " " + it->second.type;
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
        // if (result.this_block->block_num == 1000000)
        //     throw std::runtime_error("stop");

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

        auto num = read_varuint32(bin);
        // printf("    %u\n", num);
        unsigned numRows = 0;
        for (uint32_t i = 0; i < num; ++i) {
            check_variant(bin, get_type("table_delta"), "table_delta_v0");
            table_delta_v0 table_delta;
            if (!bin_to_native(table_delta, bin))
                throw runtime_error("table_delta conversion error (1)");
            // printf("        %s %u\n", table_delta.name.c_str(), (unsigned)table_delta.rows.size());

            auto& variant_type = get_type(table_delta.name);
            if (!variant_type.filled_variant || variant_type.fields.size() != 1 || !variant_type.fields[0].type->filled_struct)
                throw std::runtime_error("don't know how to proccess " + variant_type.name);
            auto& type = *variant_type.fields[0].type;

            for (auto& row : table_delta.rows) {
                // printf("            present:%u row bytes:%u\n", row.present, unsigned(row.data.end - row.data.pos));

                check_variant(row.data, variant_type, 0u);

                string fields;
                string values;
                for (auto& f : type.fields) {
                    if (f.type->filled_struct || f.type->filled_variant) {
                        string dummy;
                        if (!bin_to_json(row.data, f.type, dummy))
                            throw runtime_error("table_delta conversion error (2)");
                        // printf("                %s\n", dummy.c_str());
                        continue;
                    }

                    auto abi_type    = f.type->name;
                    bool is_optional = false;
                    if (abi_type.size() >= 1 && abi_type.back() == '?') {
                        is_optional = true;
                        abi_type.resize(abi_type.size() - 1);
                    }
                    auto it = sql_types.find(abi_type);
                    if (it == sql_types.end())
                        throw std::runtime_error("don't know sql type for abi type: " + abi_type);
                    if (!it->second.bin_to_sql)
                        throw std::runtime_error("don't know how to process " + f.type->name);

                    if (!fields.empty()) {
                        fields += ", ";
                        values += ", ";
                    }
                    fields += t.quote_name(f.name);
                    if (!is_optional || read_bin<bool>(row.data))
                        values += it->second.bin_to_sql(row.data);
                    else
                        values += "null";
                }

                // printf("%s\n", ("insert into " + schema + "." + table_delta.name + "(" + fields + ") values (" + values + ")").c_str());
                pipeline.insert("insert into " + schema + "." + table_delta.name + "(" + fields + ") values (" + values + ")");
            }
            numRows += table_delta.rows.size();
        }
        // printf("    numRows: %u\n", numRows);
    } // receive_deltas

    void receive_traces(uint32_t block_num, input_buffer buf, pqxx::work& t, pqxx::pipeline& pipeline) {
        auto         data = zlib_decompress(buf);
        input_buffer bin{data.data(), data.data() + data.size()};
        auto         num = read_varuint32(bin);
        for (uint32_t i = 0; i < num; ++i) {
            transaction_trace ttrace;
            if (!bin_to_native(ttrace, bin))
                throw runtime_error("transaction_trace conversion error (1)");
            printf("trace\n");
        }
    } // receive_traces

    void send_request() {
        send(jvalue{jarray{{"get_blocks_request_v0"s},
                           {jobject{
                               {{"start_block_num"s}, {"0"s}},
                               {{"end_block_num"s}, {"4294967295"s}},
                               {{"max_messages_in_flight"s}, {"4294967295"s}},
                               {{"have_positions"s}, {jarray{}}},
                               {{"irreversible_only"s}, {false}},
                               {{"fetch_block"s}, {false}},
                               {{"fetch_traces"s}, {false}},
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
