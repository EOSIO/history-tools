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
    virtual ~connection_callbacks() = default;
    virtual void received_abi(std::string_view abi) {}
    virtual bool received(get_status_result_v0& status) { return true; }
    virtual bool received(get_blocks_result_v0& result) { return true; }
    virtual void closed() = 0;
};

struct connection_config {
    std::string host;
    std::string port;
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
        auto                  data = p->data();
        input_buffer          bin{(const char*)data.data(), (const char*)data.data() + data.size()};
        state_history::result result;
        bin_to_native(result, bin);
        return callbacks && std::visit([&](auto& r) { return callbacks->received(r); }, result);
    }

    void request_blocks(uint32_t start_block_num, const std::vector<block_position>& positions) {
        get_blocks_request_v0 req;
        req.start_block_num        = start_block_num;
        req.end_block_num          = 0xffff'ffff;
        req.max_messages_in_flight = 0xffff'ffff;
        req.have_positions         = positions;
        req.irreversible_only      = false;
        req.fetch_block            = true;
        req.fetch_traces           = true;
        req.fetch_deltas           = true;
        send(req);
    }

    void request_blocks(const get_status_result_v0& status, uint32_t start_block_num, const std::vector<block_position>& positions) {
        uint32_t nodeos_start = 0xffff'ffff;
        if (status.trace_begin_block < status.trace_end_block)
            nodeos_start = std::min(nodeos_start, status.trace_begin_block);
        if (status.chain_state_begin_block < status.chain_state_end_block)
            nodeos_start = std::min(nodeos_start, status.chain_state_begin_block);
        if (nodeos_start == 0xffff'ffff)
            nodeos_start = 0;
        request_blocks(std::max(start_block_num, nodeos_start), positions);
    }

    const abi_type& get_type(const std::string& name) {
        auto it = abi_types.find(name);
        if (it == abi_types.end())
            throw std::runtime_error(std::string("unknown type ") + name);
        return it->second;
    }

    void send(const request& req) {
        auto bin = std::make_shared<std::vector<char>>();
        abieos::native_to_bin(*bin, req);
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
