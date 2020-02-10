// copyright defined in LICENSE.txt

// Adapted from Boost Beast Advanced Server example
//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "wasm_ql_http.hpp"

#include <eosio/from_json.hpp>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/make_unique.hpp>
#include <boost/optional.hpp>

#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http  = beast::http;          // from <boost/beast/http.hpp>
namespace net   = boost::asio;          // from <boost/asio.hpp>
using tcp       = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

using namespace std::literals;

namespace wasm_ql {

class thread_state_cache {
  private:
    std::mutex                                   mutex;
    std::shared_ptr<const wasm_ql::shared_state> shared_state;
    std::vector<std::unique_ptr<thread_state>>   states;

  public:
    thread_state_cache(const std::shared_ptr<const wasm_ql::shared_state>& shared_state)
        : shared_state(shared_state) {}

    std::unique_ptr<thread_state> get_state() {
        std::lock_guard<std::mutex> lock{mutex};
        if (states.empty()) {
            auto result    = std::make_unique<thread_state>();
            result->shared = shared_state;
            return result;
        }
        auto result = std::move(states.back());
        states.pop_back();
        return result;
    }

    void store_state(std::unique_ptr<thread_state> state) {
        std::lock_guard<std::mutex> lock{mutex};
        states.push_back(std::move(state));
    }
};

// Report a failure
static void fail(beast::error_code ec, const char* what) { elog("${w}: ${s}", ("w", what)("s", ec.message())); }

// Return a reasonable mime type based on the extension of a file.
beast::string_view mime_type(beast::string_view path) {
    using beast::iequals;
    const auto ext = [&path] {
        const auto pos = path.rfind(".");
        if (pos == beast::string_view::npos)
            return beast::string_view{};
        return path.substr(pos);
    }();
    if (iequals(ext, ".htm"))
        return "text/html";
    if (iequals(ext, ".html"))
        return "text/html";
    if (iequals(ext, ".php"))
        return "text/html";
    if (iequals(ext, ".css"))
        return "text/css";
    if (iequals(ext, ".txt"))
        return "text/plain";
    if (iequals(ext, ".js"))
        return "application/javascript";
    if (iequals(ext, ".json"))
        return "application/json";
    if (iequals(ext, ".wasm"))
        return "application/wasm";
    if (iequals(ext, ".xml"))
        return "application/xml";
    if (iequals(ext, ".swf"))
        return "application/x-shockwave-flash";
    if (iequals(ext, ".flv"))
        return "video/x-flv";
    if (iequals(ext, ".png"))
        return "image/png";
    if (iequals(ext, ".jpe"))
        return "image/jpeg";
    if (iequals(ext, ".jpeg"))
        return "image/jpeg";
    if (iequals(ext, ".jpg"))
        return "image/jpeg";
    if (iequals(ext, ".gif"))
        return "image/gif";
    if (iequals(ext, ".bmp"))
        return "image/bmp";
    if (iequals(ext, ".ico"))
        return "image/vnd.microsoft.icon";
    if (iequals(ext, ".tiff"))
        return "image/tiff";
    if (iequals(ext, ".tif"))
        return "image/tiff";
    if (iequals(ext, ".svg"))
        return "image/svg+xml";
    if (iequals(ext, ".svgz"))
        return "image/svg+xml";
    return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string path_cat(beast::string_view base, beast::string_view path) {
    if (base.empty())
        return std::string(path);
    std::string result(base);
#ifdef BOOST_MSVC
    char constexpr path_separator = '\\';
    if (result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    for (auto& c : result)
        if (c == '/')
            c = path_separator;
#else
    char constexpr path_separator = '/';
    if (result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
#endif
    return result;
}

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template <class Body, class Allocator, class Send>
void handle_request(
    beast::string_view doc_root, const std::shared_ptr<const shared_state>& shared_state,
    const std::shared_ptr<thread_state_cache>& state_cache, http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    // Returns a bad request response
    const auto bad_request = [&req](beast::string_view why) {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = why.to_string();
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    const auto not_found = [&req](beast::string_view target) {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + target.to_string() + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns an error response
    const auto error = [&req](http::status status, beast::string_view why) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = why.to_string();
        res.prepare_payload();
        return res;
    };

    const auto ok = [&shared_state, &req](std::vector<char> reply, const char* content_type) {
        http::response<http::vector_body<char>> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, content_type);
        if (!shared_state->allow_origin.empty())
            res.set(http::field::access_control_allow_origin, shared_state->allow_origin);
        res.keep_alive(req.keep_alive());
        res.body() = std::move(reply);
        res.prepare_payload();
        return res;
    };

    // todo: pack error messages in json
    // todo: replace "query failed"
    try {
        if (req.target() == "/v1/chain/get_info") {
            auto thread_state = state_cache->get_state();
            send(ok(query_get_info(*thread_state), "application/json"));
            state_cache->store_state(std::move(thread_state));
            return;
        } else if (req.target() == "/v1/chain/get_block") { // todo: replace with /v1/chain/get_block_header. upgrade cleos.
            if (req.method() != http::verb::post)
                return send(error(http::status::bad_request, "Unsupported HTTP-method for " + req.target().to_string() + "\n"));
            auto thread_state = state_cache->get_state();
            send(ok(query_get_block(*thread_state, std::string_view{req.body().data(), req.body().size()}), "application/json"));
            state_cache->store_state(std::move(thread_state));
            return;
        } else if (req.target() == "/v1/chain/get_abi") { // todo: get_raw_abi. upgrade cleos to use get_raw_abi.
            if (req.method() != http::verb::post)
                return send(error(http::status::bad_request, "Unsupported HTTP-method for " + req.target().to_string() + "\n"));
            auto thread_state = state_cache->get_state();
            send(ok(query_get_abi(*thread_state, std::string_view{req.body().data(), req.body().size()}), "application/json"));
            state_cache->store_state(std::move(thread_state));
            return;
        } else if (req.target() == "/v1/chain/send_transaction") {
            // todo: replace with /v1/chain/send_transaction2?
            // or:   change nodeos to not do abi deserialization if transaction extension present?
            if (req.method() != http::verb::post)
                return send(error(http::status::bad_request, "Unsupported HTTP-method for " + req.target().to_string() + "\n"));
            auto thread_state = state_cache->get_state();
            send(ok(query_send_transaction(*thread_state, std::string_view{req.body().data(), req.body().size()}), "application/json"));
            state_cache->store_state(std::move(thread_state));
            return;
        } else if (req.target().starts_with("/v1/") || doc_root.empty()) {
            // todo: redirect if /v1/?
            return send(error(http::status::not_found, "The resource '" + req.target().to_string() + "' was not found.\n"));
        } else {
            // Make sure we can handle the method
            if (req.method() != http::verb::get && req.method() != http::verb::head)
                return send(bad_request("Unknown HTTP-method"));

            // Request path must be absolute and not contain "..".
            if (req.target().empty() || req.target()[0] != '/' || req.target().find("..") != beast::string_view::npos)
                return send(bad_request("Illegal request-target"));

            // Build the path to the requested file
            std::string path = path_cat(doc_root, req.target());
            if (req.target().back() == '/')
                path.append("index.html");

            // Attempt to open the file
            beast::error_code           ec;
            http::file_body::value_type body;
            body.open(path.c_str(), beast::file_mode::scan, ec);

            // Handle the case where the file doesn't exist
            if (ec == beast::errc::no_such_file_or_directory)
                return send(not_found(req.target()));

            // Handle an unknown error
            if (ec)
                return send(error(http::status::internal_server_error, "An error occurred: "s + ec.message()));

            // Cache the size since we need it after the move
            const auto size = body.size();

            // Respond to HEAD request
            if (req.method() == http::verb::head) {
                http::response<http::empty_body> res{http::status::ok, req.version()};
                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                res.set(http::field::content_type, mime_type(path));
                res.content_length(size);
                res.keep_alive(req.keep_alive());
                return send(std::move(res));
            }

            // Respond to GET request
            http::response<http::file_body> res{std::piecewise_construct, std::make_tuple(std::move(body)),
                                                std::make_tuple(http::status::ok, req.version())};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, mime_type(path));
            res.content_length(size);
            res.keep_alive(req.keep_alive());
            return send(std::move(res));
        }
    } catch (const std::exception& e) {
        elog("query failed: ${s}", ("s", e.what()));
        return send(error(http::status::internal_server_error, "query failed: "s + e.what() + "\n"));
    } catch (...) {
        elog("query failed: unknown exception");
        return send(error(http::status::internal_server_error, "query failed: unknown exception\n"));
    }
}

// Handles an HTTP server connection
class http_session : public std::enable_shared_from_this<http_session> {
    // This queue is used for HTTP pipelining.
    class queue {
        enum {
            // Maximum number of responses we will queue
            limit = 8
        };

        // The type-erased, saved work item
        struct work {
            virtual ~work()           = default;
            virtual void operator()() = 0;
        };

        http_session&                      self_;
        std::vector<std::unique_ptr<work>> items_;

      public:
        explicit queue(http_session& self)
            : self_(self) {
            static_assert(limit > 0, "queue limit must be positive");
            items_.reserve(limit);
        }

        // Returns `true` if we have reached the queue limit
        bool is_full() const { return items_.size() >= limit; }

        // Called when a message finishes sending
        // Returns `true` if the caller should initiate a read
        bool on_write() {
            BOOST_ASSERT(!items_.empty());
            const auto was_full = is_full();
            items_.erase(items_.begin());
            if (!items_.empty())
                (*items_.front())();
            return was_full;
        }

        // Called by the HTTP handler to send a response.
        template <bool isRequest, class Body, class Fields>
        void operator()(http::message<isRequest, Body, Fields>&& msg) {
            // This holds a work item
            struct work_impl : work {
                http_session&                          self_;
                http::message<isRequest, Body, Fields> msg_;

                work_impl(http_session& self, http::message<isRequest, Body, Fields>&& msg)
                    : self_(self)
                    , msg_(std::move(msg)) {}

                void operator()() {
                    http::async_write(
                        self_.stream_, msg_, beast::bind_front_handler(&http_session::on_write, self_.shared_from_this(), msg_.need_eof()));
                }
            };

            // Allocate and store the work
            items_.push_back(boost::make_unique<work_impl>(self_, std::move(msg)));

            // If there was no previous work, start this one
            if (items_.size() == 1)
                (*items_.front())();
        }
    };

    beast::tcp_stream                   stream_;
    beast::flat_buffer                  buffer_;
    std::shared_ptr<const std::string>  doc_root_;
    std::shared_ptr<const shared_state> shared_state_;
    std::shared_ptr<thread_state_cache> state_cache_;
    queue                               queue_;

    // The parser is stored in an optional container so we can
    // construct it from scratch it at the beginning of each new message.
    boost::optional<http::request_parser<http::vector_body<char>>> parser_;

  public:
    // Take ownership of the socket
    http_session(
        tcp::socket&& socket, const std::shared_ptr<const std::string>& doc_root, const std::shared_ptr<const shared_state>& shared_state,
        const std::shared_ptr<thread_state_cache>& state_cache)
        : stream_(std::move(socket))
        , doc_root_(doc_root)
        , shared_state_(shared_state)
        , state_cache_(state_cache)
        , queue_(*this) {}

    // Start the session
    void run() { do_read(); }

  private:
    void do_read() {
        // Construct a new parser for each message
        parser_.emplace();

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        // todo: make configurable
        parser_->body_limit(10000);

        // Set the timeout.
        stream_.expires_after(std::chrono::seconds(30));

        // Read a request using the parser-oriented interface
        http::async_read(stream_, buffer_, *parser_, beast::bind_front_handler(&http_session::on_read, shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if (ec == http::error::end_of_stream)
            return do_close();

        if (ec)
            return fail(ec, "read");

        // Send the response
        handle_request(*doc_root_, shared_state_, state_cache_, parser_->release(), queue_);

        // If we aren't at the queue limit, try to pipeline another request
        if (!queue_.is_full())
            do_read();
    }

    void on_write(bool close, beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        if (close) {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return do_close();
        }

        // Inform the queue that a write completed
        if (queue_.on_write()) {
            // Read another request
            do_read();
        }
    }

    void do_close() {
        // Send a TCP shutdown
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }
};

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener> {
    net::io_context&                    ioc_;
    tcp::acceptor                       acceptor_;
    bool                                acceptor_ready = false;
    std::shared_ptr<const std::string>  doc_root_;
    std::shared_ptr<const shared_state> shared_state_;
    std::shared_ptr<thread_state_cache> state_cache_;

  public:
    listener(
        net::io_context& ioc, tcp::endpoint endpoint, const std::shared_ptr<const std::string>& doc_root,
        const std::shared_ptr<const shared_state>& shared_state)
        : ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , doc_root_(doc_root)
        , shared_state_(shared_state)
        , state_cache_(std::make_shared<thread_state_cache>(shared_state_)) {

        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            fail(ec, "open");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec) {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            fail(ec, "listen");
            return;
        }

        acceptor_ready = true;
    }

    // Start accepting incoming connections
    void run() {
        if (acceptor_ready)
            do_accept();
    }

  private:
    void do_accept() {
        // The new connection gets its own strand
        acceptor_.async_accept(net::make_strand(ioc_), beast::bind_front_handler(&listener::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (ec) {
            fail(ec, "accept");
        } else {
            // Create the http session and run it
            std::make_shared<http_session>(std::move(socket), doc_root_, shared_state_, state_cache_)->run();
        }

        // Accept another connection
        do_accept();
    }
};

struct server_impl : http_server, std::enable_shared_from_this<server_impl> {
    int                                 num_threads;
    net::io_service                     ioc;
    std::shared_ptr<const shared_state> state    = {};
    std::string                         address  = {};
    std::string                         port     = {};
    std::vector<std::thread>            threads  = {};
    std::unique_ptr<tcp::acceptor>      acceptor = {};

    server_impl(int num_threads, const std::shared_ptr<const shared_state>& state, const std::string& address, const std::string& port)
        : num_threads{num_threads}
        , ioc{num_threads}
        , state{state}
        , address{address}
        , port{port} {}

    virtual ~server_impl() {}

    virtual void stop() override {
        ioc.stop();
        for (auto& t : threads)
            t.join();
        threads.clear();
    }

    void start() {
        boost::system::error_code ec;
        auto                      check_ec = [&](const char* what) {
            if (!ec)
                return;
            elog("${w}: ${m}", ("w", what)("m", ec.message()));
            FC_ASSERT(false, "unable to open listen socket");
        };

        ilog("listen on ${a}:${p}", ("a", address)("p", port));
        boost::asio::ip::address a;
        try {
            a = net::ip::make_address(address);
        } catch (std::exception& e) {
            throw std::runtime_error("make_address(): "s + address + ": " + e.what());
        }
        std::make_shared<listener>(
            ioc, tcp::endpoint{a, (unsigned short)std::atoi(port.c_str())}, std::make_shared<std::string>(state->static_dir), state)
            ->run();

        threads.reserve(num_threads);
        for (int i = 0; i < num_threads; ++i)
            threads.emplace_back([self = shared_from_this()] { self->ioc.run(); });
    }
}; // server_impl

std::shared_ptr<http_server> http_server::create(
    int num_threads, const std::shared_ptr<const shared_state>& state, const std::string& address, const std::string& port) {
    FC_ASSERT(num_threads > 0, "too few threads");
    auto server = std::make_shared<server_impl>(num_threads, state, address, port);
    server->start();
    return server;
}

} // namespace wasm_ql
