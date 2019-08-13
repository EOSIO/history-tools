// copyright defined in LICENSE.txt

#include "wasm_ql_http.hpp"

#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>
#include <boost/filesystem.hpp>

#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
using tcp       = asio::ip::tcp;
using beast::tcp_stream;

using namespace std::literals;

namespace wasm_ql {

static void fail(beast::error_code ec, char const* what) { elog("${w}: ${s}", ("w", what)("s", ec.message())); }

template <typename F>
static auto catch_and_log(F f) {
    try {
        return f();
    } catch (const fc::exception& e) {
        elog("${e}", ("e", e.to_detail_string()));
    } catch (const std::exception& e) {
        elog("${e}", ("e", e.what()));
    } catch (...) {
        elog("unknown exception");
    }
}

static const char* get_content_type(const boost::filesystem::path& ext) {
    if (ext == ".htm")
        return "text/html";
    else if (ext == ".html")
        return "text/html";
    else if (ext == ".css")
        return "text/css";
    else if (ext == ".txt")
        return "text/plain";
    else if (ext == ".js")
        return "application/javascript";
    else if (ext == ".json")
        return "application/json";
    else if (ext == ".wasm")
        return "application/wasm";
    else if (ext == ".xml")
        return "application/xml";
    else if (ext == ".png")
        return "image/png";
    else if (ext == ".jpe")
        return "image/jpeg";
    else if (ext == ".jpeg")
        return "image/jpeg";
    else if (ext == ".jpg")
        return "image/jpeg";
    else if (ext == ".gif")
        return "image/gif";
    else if (ext == ".bmp")
        return "image/bmp";
    else if (ext == ".ico")
        return "image/vnd.microsoft.icon";
    else if (ext == ".tiff")
        return "image/tiff";
    else if (ext == ".tif")
        return "image/tiff";
    else if (ext == ".svg")
        return "image/svg+xml";
    else if (ext == ".svgz")
        return "image/svg+xml";
    else
        return "application/octet-stream";
}

static void
handle_request(wasm_ql::thread_state& thread_state, tcp_stream& socket, http::request<http::vector_body<char>> req, beast::error_code& ec) {

    auto const error = [&req](http::status status, beast::string_view why) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        // res.keep_alive(req.keep_alive());
        res.body() = why.to_string();
        res.prepare_payload();
        return res;
    };

    auto const ok = [&thread_state, &req](std::vector<char> reply, const char* content_type) {
        http::response<http::vector_body<char>> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, content_type);
        res.set(http::field::access_control_allow_origin, thread_state.shared->allow_origin);
        // res.keep_alive(req.keep_alive());
        res.body() = std::move(reply);
        res.prepare_payload();
        return res;
    };

    auto const file = [&thread_state, &req, &ec](const boost::filesystem::path& path) {
        http::file_body::value_type body;
        body.open(path.c_str(), beast::file_mode::scan, ec);
        auto                            size = body.size();
        http::response<http::file_body> res{std::piecewise_construct, std::make_tuple(std::move(body)),
                                            std::make_tuple(http::status::ok, req.version())};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, get_content_type(path.extension()));
        res.set(http::field::access_control_allow_origin, thread_state.shared->allow_origin);
        res.content_length(size);
        // res.keep_alive(req.keep_alive());
        res.prepare_payload();
        return res;
    };

    auto send = [&](const auto& msg) {
        http::serializer<false, typename std::decay_t<decltype(msg)>::body_type> sr{const_cast<std::decay_t<decltype(msg)>&>(msg)};
        http::write(socket, sr, ec);
    };

    auto target = req.target();
    try {
        if (target == "/wasmql/v1/query") {
            if (req.method() != http::verb::post)
                return send(error(http::status::bad_request, "Unsupported HTTP-method for " + (std::string)target + "\n"));
            return send(ok(query(thread_state, req.body()), "application/octet-stream"));
        } else if (target.starts_with("/v1/")) {
            if (req.method() != http::verb::post)
                return send(error(http::status::bad_request, "Unsupported HTTP-method for " + (std::string)target + "\n"));
            return send(ok(legacy_query(thread_state, std::string(target.begin(), target.end()), req.body()), "application/octet-stream"));
        } else if (!thread_state.shared->static_dir.empty()) {
            if (req.method() != http::verb::get)
                return send(error(http::status::bad_request, "Unsupported HTTP-method for " + (std::string)target + "\n"));
            auto s = (std::string)target;
            while (!s.empty() && s[0] == '/')
                s.erase(s.begin(), s.begin() + 1);
            try {
                auto base      = boost::filesystem::canonical(thread_state.shared->static_dir);
                auto abs       = boost::filesystem::canonical(s, base);
                auto base_size = std::distance(base.begin(), base.end());
                auto abs_size  = std::distance(abs.begin(), abs.end());
                if (abs_size >= base_size) {
                    auto end = abs.begin();
                    std::advance(end, base_size);
                    if (lexicographical_compare(base.begin(), base.end(), abs.begin(), end) == 0) {
                        if (abs == base)
                            abs = boost::filesystem::canonical("index.html", base);
                        return send(file(abs));
                    }
                }
            } catch (...) {
                return send(error(http::status::not_found, "The resource '" + req.target().to_string() + "' was not found.\n"));
            }
        }
    } catch (const std::exception& e) {
        elog("query failed: ${s}", ("s", e.what()));
        return send(error(http::status::internal_server_error, "query failed: "s + e.what() + "\n"));
    } catch (...) {
        elog("query failed: unknown exception");
        return send(error(http::status::internal_server_error, "query failed: unknown exception\n"));
    }

    return send(error(http::status::not_found, "The resource '" + req.target().to_string() + "' was not found.\n"));
} // handle_request

static void accepted(const std::shared_ptr<const wasm_ql::shared_state>& shared_state, tcp_stream socket) {
    beast::error_code     ec;
    wasm_ql::thread_state thread_state{shared_state};

    beast::flat_buffer buffer;
    for (;;) {
        http::request<http::vector_body<char>> req;
        http::read(socket, buffer, req, ec);
        if (ec == http::error::end_of_stream)
            break;
        if (ec)
            return fail(ec, "read");

        handle_request(thread_state, socket, std::move(req), ec);
        if (ec)
            return fail(ec, "write");
        break; // disable keep-alive support for now
    }
    // socket.shutdown(tcp::socket::shutdown_send, ec);
}

struct server_impl : http_server, std::enable_shared_from_this<server_impl> {
    int                                 num_threads;
    asio::io_service                    ioc;
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
        tcp::resolver resolver(ioc);
        auto          addr = resolver.resolve(tcp::resolver::query(tcp::v4(), address, port));
        acceptor           = std::make_unique<tcp::acceptor>(boost::asio::make_strand(ioc));
        acceptor->open(addr->endpoint().protocol());
        int x = 1;
        setsockopt(acceptor->native_handle(), SOL_SOCKET, SO_REUSEPORT, &x, sizeof(x));
        acceptor->bind(addr->endpoint());
        acceptor->listen(1, ec);
        check_ec("listen");
        do_accept();

        threads.reserve(num_threads);
        for (int i = 0; i < num_threads; ++i)
            threads.emplace_back([self = shared_from_this()] { self->ioc.run(); });
    }

    void do_accept() {
        acceptor->async_accept(boost::asio::make_strand(ioc), [self = shared_from_this()](auto ec, auto socket) {
            if (self->ioc.stopped())
                return;
            if (ec) {
                if (ec == boost::system::errc::too_many_files_open)
                    catch_and_log([&] { self->do_accept(); });
                return;
            }
            catch_and_log([&] { accepted(self->state, tcp_stream{std::move(socket)}); });
            catch_and_log([&] { self->do_accept(); });
        });
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
