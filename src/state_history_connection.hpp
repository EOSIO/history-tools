// copyright defined in LICENSE.txt

#pragma once

#include "state_history.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <fc/exception/exception.hpp>

namespace state_history {

struct connection_callbacks {
    virtual ~connection_callbacks()                            = default;
    virtual void received_abi(std::string_view abi)            = 0;
    virtual bool received_result(get_blocks_result_v0& result) = 0;
    virtual void closed()                                      = 0;
};

struct connection_config {
    std::string host;
    std::string port;
    uint32_t    stop_before = 0;
};

struct connection : std::enable_shared_from_this<connection> {
    using error_code  = boost::system::error_code;
    using flat_buffer = boost::beast::flat_buffer;
    using tcp         = boost::asio::ip::tcp;

    using abi_def      = abieos::abi_def;
    using abi_type     = abieos::abi_type;
    using input_buffer = abieos::input_buffer;
    using jarray       = abieos::jarray;
    using jobject      = abieos::jobject;
    using jvalue       = abieos::jvalue;

    connection_config                            config;
    std::shared_ptr<connection_callbacks>        callbacks;
    tcp::resolver                                resolver;
    boost::beast::websocket::stream<tcp::socket> stream;
    bool                                         have_abi  = false;
    abi_def                                      abi       = {};
    std::map<std::string, abi_type>              abi_types = {};

    connection(boost::asio::io_context& ioc, const connection_config& config, std::shared_ptr<connection_callbacks> callbacks)
        : config(config)
        , callbacks(callbacks)
        , resolver(ioc)
        , stream(ioc) {

        stream.binary(true);
        stream.read_message_max(10ull * 1024 * 1024 * 1024);
    }

    void connect() {
        ilog("connect to ${h}:${p}", ("h", config.host)("p", config.port));
        resolver.async_resolve(
            config.host, config.port, [self = shared_from_this(), this](error_code ec, tcp::resolver::results_type results) {
                enter_callback(ec, "resolve", [&] {
                    boost::asio::async_connect(
                        stream.next_layer(), results.begin(), results.end(), [self = shared_from_this(), this](error_code ec, auto&) {
                            enter_callback(ec, "connect", [&] {
                                stream.async_handshake(config.host, "/", [self = shared_from_this(), this](error_code ec) {
                                    enter_callback(ec, "handshake", [&] { //
                                        start_read();
                                    });
                                });
                            });
                        });
                });
            });
    }

    void start_read() {
        auto in_buffer = std::make_shared<flat_buffer>();
        stream.async_read(*in_buffer, [self = shared_from_this(), this, in_buffer](error_code ec, size_t) {
            enter_callback(ec, "async_read", [&] {
                if (!have_abi)
                    receive_abi(in_buffer);
                else {
                    if (!receive_result(in_buffer)) {
                        close();
                        return;
                    }
                }
                start_read();
            });
        });
    }

    void receive_abi(const std::shared_ptr<flat_buffer>& p) {
        auto data = p->data();
        auto sv   = std::string_view{(const char*)data.data(), data.size()};
        json_to_native(abi, sv);
        abieos::check_abi_version(abi.version);
        abi_types = abieos::create_contract(abi).abi_types;
        have_abi  = true;
        if (callbacks)
            callbacks->received_abi(sv);
    }

    bool receive_result(const std::shared_ptr<flat_buffer>& p) {
        auto         data = p->data();
        input_buffer bin{(const char*)data.data(), (const char*)data.data() + data.size()};
        check_variant(bin, get_type("result"), "get_blocks_result_v0");
        get_blocks_result_v0 result;
        bin_to_native(result, bin);
        if (!result.this_block)
            return true;
        if (config.stop_before && result.this_block->block_num >= config.stop_before) {
            ilog("block ${b}: stop requested", ("b", result.this_block->block_num));
            return false;
        }
        return callbacks && callbacks->received_result(result);
    }

    void send_request(uint32_t start_block_num, const jarray& positions) {
        using namespace std::literals;
        send(jvalue{jarray{{"get_blocks_request_v0"s},
                           {jobject{
                               {{"start_block_num"s}, {std::to_string(start_block_num)}},
                               {{"end_block_num"s}, {"4294967295"s}},
                               {{"max_messages_in_flight"s}, {"4294967295"s}},
                               {{"have_positions"s}, {positions}},
                               {{"irreversible_only"s}, {false}},
                               {{"fetch_block"s}, {true}},
                               {{"fetch_traces"s}, {true}},
                               {{"fetch_deltas"s}, {true}},
                           }}}});
    }

    const abi_type& get_type(const std::string& name) {
        auto it = abi_types.find(name);
        if (it == abi_types.end())
            throw std::runtime_error(std::string("unknown type ") + name);
        return it->second;
    }

    void send(const jvalue& value) {
        auto bin = std::make_shared<std::vector<char>>();
        json_to_bin(*bin, &get_type("request"), value);
        stream.async_write(boost::asio::buffer(*bin), [self = shared_from_this(), bin, this](error_code ec, size_t) {
            enter_callback(ec, "async_write", [&] {});
        });
    }

    template <typename F>
    void catch_and_close(F f) {
        try {
            f();
        } catch (const std::exception& e) {
            elog("${e}", ("e", e.what()));
            close();
        } catch (...) {
            elog("unknown exception");
            close();
        }
    }

    template <typename F>
    void enter_callback(error_code ec, const char* what, F f) {
        if (ec)
            return on_fail(ec, what);
        catch_and_close(f);
    }

    void on_fail(error_code ec, const char* what) {
        try {
            elog("${w}: ${m}", ("w", what)("m", ec.message()));
            close();
        } catch (...) {
            elog("exception while closing");
        }
    }

    void close() {
        ilog("closing state-history socket");
        stream.next_layer().close();
        if (callbacks)
            callbacks->closed();
        callbacks.reset();
    }
}; // connection

} // namespace state_history
