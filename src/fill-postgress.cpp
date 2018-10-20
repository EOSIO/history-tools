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
using std::vector;

namespace asio      = boost::asio;
namespace bio       = boost::iostreams;
namespace websocket = boost::beast::websocket;

using asio::ip::tcp;
using boost::beast::flat_buffer;
using boost::system::error_code;

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
    tcp::resolver                  resolver;
    websocket::stream<tcp::socket> stream;
    string                         host;
    string                         port;
    bool                           received_abi = false;
    abi_def                        abi{};
    map<string, abi_type>          abi_types;

    explicit session(asio::io_context& ioc, string host, string port)
        : resolver(ioc)
        , stream(ioc)
        , host(move(host))
        , port(move(port)) {
        stream.binary(true);
    }

    void start() {
        resolver.async_resolve(
            host, port, [self = shared_from_this(), this](error_code ec, tcp::resolver::results_type results) {
                callback(ec, "resolve", [&] {
                    asio::async_connect(
                        stream.next_layer(), results.begin(), results.end(),
                        [self = shared_from_this(), this](error_code ec, auto&) {
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
        send_request();
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
        printf("block %d\n", result.this_block->block_num);
        if (result.deltas)
            receive_deltas(result.this_block->block_num, *result.deltas);
    }

    void receive_deltas(uint32_t block_num, input_buffer buf) {
        auto         data = zlib_decompress(buf);
        input_buffer bin{data.data(), data.data() + data.size()};

        auto num = read_varuint32(bin);
        printf("    %u\n", num);
        unsigned numRows = 0;
        for (uint32_t i = 0; i < num; ++i) {
            check_variant(bin, get_type("table_delta"), "table_delta_v0");
            table_delta_v0 table_delta;
            if (!bin_to_native(table_delta, bin))
                throw runtime_error("table_delta conversion error");
            printf("        %s %u\n", table_delta.name.c_str(), (unsigned)table_delta.rows.size());
            for (auto& r : table_delta.rows)
                printf("            present:%u row bytes:%u\n", r.present, unsigned(r.data.end - r.data.pos));
            numRows += table_delta.rows.size();
        }
        printf("    numRows: %u\n", numRows);
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
                               {{"fetch_traces"s}, {false}},
                               {{"fetch_deltas"s}, {true}},
                           }}}});
    }

    const abi_type& get_type(const char* name) {
        auto it = abi_types.find(name);
        if (it == abi_types.end())
            throw runtime_error("unknown type "s + name);
        return it->second;
    }

    void send(const jvalue& value) {
        auto bin = make_shared<vector<char>>();
        if (!json_to_bin(*bin, &get_type("request"), value))
            throw runtime_error("failed to convert during send");

        stream.async_write(asio::buffer(*bin), [self = shared_from_this(), bin, this](error_code ec, size_t) {
            callback(ec, "async_write", [&] {});
        });
    }

    void check_variant(input_buffer& bin, const abi_type& type, const char* expected) {
        auto index = read_varuint32(bin);
        if (!type.filled_variant)
            throw runtime_error(expected + " is type"s);
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
    if (argc != 3) {
        cerr << "Usage: websocket-client-async <host> <port>\n"
             << "Example:\n"
             << "    websocket-client-async localhost 8080\n";
        return EXIT_FAILURE;
    }
    auto const host = argv[1];
    auto const port = argv[2];

    asio::io_context ioc;
    make_shared<session>(ioc, host, port)->start();
    ioc.run();

    return EXIT_SUCCESS;
}
