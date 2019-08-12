// copyright defined in LICENSE.txt

// todo: timeout wasm
// todo: timeout sql
// todo: what should memory size limit be?
// todo: check callbacks for recursion to limit stack size
// todo: reformulate get_input_data and set_output_data for reentrancy
// todo: wasms get whether a query is present
// todo: indexes on authorized, ram usage, notify
// todo: namespaces for queries
//          A standard namespace
//          ? one for the tokens
// todo: version on queries
//       vector<extendable<...>>
// todo: version on query api?
// todo: better naming for queries

#include "wasm_ql.hpp"

#include "util.hpp"
#include "wasm_ql_plugin.hpp"

#include <fc/crypto/hex.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/datastream.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/http/vector_body.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/filesystem.hpp>
#include <boost/signals2/connection.hpp>
#include <fstream>

using namespace abieos;
using namespace appbase;
using namespace wasm_ql;
using namespace std::literals;
namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace ws    = boost::beast::websocket;
using tcp       = asio::ip::tcp;

static void fail(beast::error_code ec, char const* what) { elog("${w}: ${s}", ("w", what)("s", ec.message())); }

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

static void handle_request(
    wasm_ql::thread_state& thread_state, tcp::socket& socket, http::request<http::vector_body<char>> req, beast::error_code& ec) {
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
}

static void accepted(const std::shared_ptr<wasm_ql::shared_state>& shared_state, tcp::socket socket) {
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
    socket.shutdown(tcp::socket::shutdown_send, ec);
}

static abstract_plugin& _wasm_ql_plugin = app().register_plugin<wasm_ql_plugin>();

using boost::signals2::scoped_connection;

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

struct wasm_ql_plugin_impl : std::enable_shared_from_this<wasm_ql_plugin_impl> {
    bool                                   stopping = false;
    std::string                            endpoint_address;
    std::string                            endpoint_port;
    std::unique_ptr<tcp::acceptor>         acceptor;
    std::shared_ptr<wasm_ql::shared_state> state;

    void listen() {
        boost::system::error_code ec;
        auto                      check_ec = [&](const char* what) {
            if (!ec)
                return;
            elog("${w}: ${m}", ("w", what)("m", ec.message()));
            FC_ASSERT(false, "unable to open listen socket");
        };

        ilog("listen on ${a}:${p}", ("a", endpoint_address)("p", endpoint_port));
        tcp::resolver resolver(app().get_io_service());
        auto          addr = resolver.resolve(tcp::resolver::query(tcp::v4(), endpoint_address, endpoint_port));
        acceptor           = std::make_unique<tcp::acceptor>(app().get_io_service());
        acceptor->open(addr->endpoint().protocol());
        int x = 1;
        setsockopt(acceptor->native_handle(), SOL_SOCKET, SO_REUSEPORT, &x, sizeof(x));
        acceptor->bind(addr->endpoint());
        acceptor->listen(1, ec);
        check_ec("listen");
        do_accept();
    }

    void do_accept() {
        auto socket = std::make_shared<tcp::socket>(app().get_io_service());
        acceptor->async_accept(*socket, [self = shared_from_this(), socket, this](auto ec) {
            if (stopping)
                return;
            if (ec) {
                if (ec == boost::system::errc::too_many_files_open)
                    catch_and_log([&] { do_accept(); });
                return;
            }
            catch_and_log([&] { accepted(state, std::move(*socket)); });
            catch_and_log([&] { do_accept(); });
        });
    }
}; // wasm_ql_plugin_impl

wasm_ql_plugin::wasm_ql_plugin()
    : my(std::make_shared<wasm_ql_plugin_impl>()) {}

wasm_ql_plugin::~wasm_ql_plugin() {
    if (my->stopping)
        ilog("wasm_ql_plugin stopped");
}

void wasm_ql_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto op = cfg.add_options();
    op("wql-listen", bpo::value<std::string>()->default_value("localhost:8880"), "Endpoint to listen on");
    op("wql-allow-origin", bpo::value<std::string>()->default_value("*"), "Access-Control-Allow-Origin header. Use \"*\" to allow any.");
    op("wql-wasm-dir", bpo::value<std::string>()->default_value("."), "Directory to fetch WASMs from");
    op("wql-static-dir", bpo::value<std::string>(), "Directory to serve static files from (default: disabled)");
    op("wql-console", "Show console output");
}

void wasm_ql_plugin::plugin_initialize(const variables_map& options) {
    try {
        auto ip_port = options.at("wql-listen").as<std::string>();
        if (ip_port.find(':') == std::string::npos)
            throw std::runtime_error("invalid --wql-listen value: " + ip_port);

        my->state               = std::make_shared<wasm_ql::shared_state>();
        my->state->console      = options.count("wql-console");
        my->endpoint_port       = ip_port.substr(ip_port.find(':') + 1, ip_port.size());
        my->endpoint_address    = ip_port.substr(0, ip_port.find(':'));
        my->state->allow_origin = options.at("wql-allow-origin").as<std::string>();
        my->state->wasm_dir     = options.at("wql-wasm-dir").as<std::string>();
        if (options.count("wql-static-dir"))
            my->state->static_dir = options.at("wql-static-dir").as<std::string>();

        register_callbacks();
    }
    FC_LOG_AND_RETHROW()
}

void wasm_ql_plugin::plugin_startup() {
    if (!my->state->db_iface)
        throw std::runtime_error("wasm_ql_plugin needs either wasm_ql_pg_plugin or wasm_ql_rocksdb_plugin");
    my->listen();
}
void wasm_ql_plugin::plugin_shutdown() { my->stopping = true; }

void wasm_ql_plugin::set_database(std::shared_ptr<database_interface> db_iface) { my->state->db_iface = std::move(db_iface); }
