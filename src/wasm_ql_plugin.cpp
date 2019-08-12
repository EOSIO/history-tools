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

#include "wasm_ql_plugin.hpp"
#include "util.hpp"

#include <fc/crypto/hex.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/datastream.hpp>
#include <fc/scoped_exit.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/http/vector_body.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/filesystem.hpp>
#include <boost/signals2/connection.hpp>
#include <eosio/vm/backend.hpp>
#include <fstream>

using namespace abieos;
using namespace appbase;
using namespace std::literals;
namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace ws    = boost::beast::websocket;
using tcp       = asio::ip::tcp;
using eosio::vm::wasm_allocator;

struct callbacks;
using backend_t = eosio::vm::backend<callbacks>;
using rhf_t     = eosio::vm::registered_host_functions<callbacks>;

struct state {
    wasm_allocator                      wa;
    bool                                console         = {};
    std::vector<char>                   database_status = {};
    abieos::input_buffer                request         = {}; // todo: rename
    std::vector<char>                   reply           = {}; // todo: rename
    std::string                         allow_origin    = {};
    std::string                         wasm_dir        = {};
    std::string                         static_dir      = {};
    std::shared_ptr<database_interface> db_iface        = {};
    std::unique_ptr<::query_session>    query_session   = {};
    state_history::fill_status          fill_status     = {};
};

struct callbacks {
    ::state&        state;
    wasm_allocator& allocator;
    backend_t&      backend;

    void check_bounds(const char* begin, const char* end) {
        if (begin > end)
            throw std::runtime_error("bad memory");
        // todo: check bounds
    }

    char* alloc(uint32_t cb_alloc_data, uint32_t cb_alloc, uint32_t size) {
        // todo: verify cb_alloc isn't in imports
        auto result = backend.get_context().execute_func_table(
            this, eosio::vm::interpret_visitor(backend.get_context()), cb_alloc, cb_alloc_data, size);
        if (!result || !result->is_a<eosio::vm::i32_const_t>())
            throw std::runtime_error("cb_alloc returned incorrect type");
        char* begin = allocator.get_base_ptr<char>() + result->to_ui32();
        check_bounds(begin, begin + size);
        return begin;
    }

    void abort() { throw std::runtime_error("called abort"); }

    void eosio_assert_message(bool test, const char* msg, size_t msg_len) {
        // todo: pass assert message through RPC API
        if (!test)
            throw std::runtime_error("assert failed");
    }

    void get_database_status(uint32_t cb_alloc_data, uint32_t cb_alloc) {
        auto data = alloc(cb_alloc_data, cb_alloc, state.database_status.size());
        memcpy(data, state.database_status.data(), state.database_status.size());
    }

    void get_input_data(uint32_t cb_alloc_data, uint32_t cb_alloc) {
        auto data = alloc(cb_alloc_data, cb_alloc, state.request.end - state.request.pos);
        memcpy(data, state.request.pos, state.request.end - state.request.pos);
    }

    void set_output_data(const char* begin, const char* end) {
        check_bounds(begin, end);
        state.reply.assign(begin, end);
    }

    void query_database(const char* req_begin, const char* req_end, uint32_t cb_alloc_data, uint32_t cb_alloc) {
        check_bounds(req_begin, req_end);
        auto result = state.query_session->query_database({req_begin, req_end}, state.fill_status.head);
        auto data   = alloc(cb_alloc_data, cb_alloc, result.size());
        memcpy(data, result.data(), result.size());
    }

    void print_range(const char* begin, const char* end) {
        check_bounds(begin, end);
        if (state.console)
            std::cerr.write(begin, end - begin);
    }
}; // callbacks

static void fill_context_data(::state& state) {
    state.database_status.clear();
    abieos::native_to_bin(state.fill_status.head, state.database_status);
    abieos::native_to_bin(state.fill_status.head_id, state.database_status);
    abieos::native_to_bin(state.fill_status.irreversible, state.database_status);
    abieos::native_to_bin(state.fill_status.irreversible_id, state.database_status);
    abieos::native_to_bin(state.fill_status.first, state.database_status);
}

// todo: detect state.fill_status.first changing (history trim)
static bool did_fork(::state& state) {
    auto id = state.query_session->get_block_id(state.fill_status.head);
    if (!id) {
        ilog("fork detected (prev head not found)");
        return true;
    }
    if (id->value != state.fill_status.head_id.value) {
        ilog("fork detected (head_id changed)");
        return true;
    }
    return false;
}

template <typename F>
static void retry_loop(::state& state, F f) {
    int num_tries = 0;
    while (true) {
        auto exit           = fc::make_scoped_exit([&] { state.query_session.reset(); });
        state.query_session = state.db_iface->create_query_session();
        state.fill_status   = state.query_session->get_fill_status();
        if (!state.fill_status.head)
            throw std::runtime_error("database is empty");
        fill_context_data(state);
        if (f())
            return;
        if (++num_tries >= 4)
            throw std::runtime_error("too many fork events during request");
        ilog("retry request");
    }
}

static void run_query(::state& state, abieos::name wasm_name) {
    auto      code = backend_t::read_wasm(state.wasm_dir + "/" + (string)wasm_name + "-server.wasm");
    backend_t backend(code);
    callbacks cb{state, state.wa, backend};
    backend.set_wasm_allocator(&state.wa);

    rhf_t::resolve(backend.get_module());
    backend.initialize(&cb);
    backend(&cb, "env", "initialize");
    backend(&cb, "env", "run_query");
}

static std::vector<char> query(::state& state, const std::vector<char>& request) {
    std::vector<char> result;
    retry_loop(state, [&]() {
        input_buffer request_bin{request.data(), request.data() + request.size()};
        auto         num_requests = bin_to_native<varuint32>(request_bin).value;
        result.clear();
        push_varuint32(result, num_requests);
        for (uint32_t request_index = 0; request_index < num_requests; ++request_index) {
            state.request = bin_to_native<input_buffer>(request_bin);
            auto ns_name  = bin_to_native<name>(state.request);
            if (ns_name != "local"_n)
                throw std::runtime_error("unknown namespace: " + (std::string)ns_name);
            auto wasm_name = bin_to_native<name>(state.request);

            run_query(state, wasm_name);
            if (did_fork(state))
                return false;

            // elog("result: ${s} ${x}", ("s", state.reply.size())("x", fc::to_hex(state.reply)));
            push_varuint32(result, state.reply.size());
            result.insert(result.end(), state.reply.begin(), state.reply.end());
        }
        return true;
    });
    return result;
}

static const std::vector<char>& legacy_query(::state& state, const std::string& target, const std::vector<char>& request) {
    std::vector<char> req;
    abieos::native_to_bin(target, req);
    abieos::native_to_bin(request, req);
    state.request = input_buffer{req.data(), req.data() + req.size()};
    retry_loop(state, [&]() {
        run_query(state, "legacy"_n);
        return !did_fork(state);
    });
    return state.reply;
}

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

static void handle_request(::state& state, tcp::socket& socket, http::request<http::vector_body<char>> req, beast::error_code& ec) {
    auto const error = [&req](http::status status, beast::string_view why) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        // res.keep_alive(req.keep_alive());
        res.body() = why.to_string();
        res.prepare_payload();
        return res;
    };

    auto const ok = [&state, &req](std::vector<char> reply, const char* content_type) {
        http::response<http::vector_body<char>> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, content_type);
        res.set(http::field::access_control_allow_origin, state.allow_origin);
        // res.keep_alive(req.keep_alive());
        res.body() = std::move(reply);
        res.prepare_payload();
        return res;
    };

    auto const file = [&state, &req, &ec](const boost::filesystem::path& path) {
        http::file_body::value_type body;
        body.open(path.c_str(), beast::file_mode::scan, ec);
        auto                            size = body.size();
        http::response<http::file_body> res{std::piecewise_construct, std::make_tuple(std::move(body)),
                                            std::make_tuple(http::status::ok, req.version())};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, get_content_type(path.extension()));
        res.set(http::field::access_control_allow_origin, state.allow_origin);
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
            return send(ok(query(state, req.body()), "application/octet-stream"));
        } else if (target.starts_with("/v1/")) {
            if (req.method() != http::verb::post)
                return send(error(http::status::bad_request, "Unsupported HTTP-method for " + (std::string)target + "\n"));
            return send(ok(legacy_query(state, std::string(target.begin(), target.end()), req.body()), "application/octet-stream"));
        } else if (!state.static_dir.empty()) {
            if (req.method() != http::verb::get)
                return send(error(http::status::bad_request, "Unsupported HTTP-method for " + (std::string)target + "\n"));
            auto s = (std::string)target;
            while (!s.empty() && s[0] == '/')
                s.erase(s.begin(), s.begin() + 1);
            try {
                auto base      = boost::filesystem::canonical(state.static_dir);
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

static void accepted(::state& state, tcp::socket socket) {
    beast::error_code ec;

    beast::flat_buffer buffer;
    for (;;) {
        http::request<http::vector_body<char>> req;
        http::read(socket, buffer, req, ec);
        if (ec == http::error::end_of_stream)
            break;
        if (ec)
            return fail(ec, "read");

        handle_request(state, socket, std::move(req), ec);
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
    bool                           stopping = false;
    std::string                    endpoint_address;
    std::string                    endpoint_port;
    std::unique_ptr<tcp::acceptor> acceptor;
    std::unique_ptr<::state>       state;

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
            catch_and_log([&] { accepted(*state, std::move(*socket)); });
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

        my->state               = std::make_unique<::state>();
        my->state->console      = options.count("wql-console");
        my->endpoint_port       = ip_port.substr(ip_port.find(':') + 1, ip_port.size());
        my->endpoint_address    = ip_port.substr(0, ip_port.find(':'));
        my->state->allow_origin = options.at("wql-allow-origin").as<std::string>();
        my->state->wasm_dir     = options.at("wql-wasm-dir").as<std::string>();
        if (options.count("wql-static-dir"))
            my->state->static_dir = options.at("wql-static-dir").as<std::string>();

        rhf_t::add<callbacks, &callbacks::abort, wasm_allocator>("env", "abort");
        rhf_t::add<callbacks, &callbacks::eosio_assert_message, wasm_allocator>("env", "eosio_assert_message");
        rhf_t::add<callbacks, &callbacks::get_database_status, wasm_allocator>("env", "get_database_status");
        rhf_t::add<callbacks, &callbacks::get_input_data, wasm_allocator>("env", "get_input_data");
        rhf_t::add<callbacks, &callbacks::set_output_data, wasm_allocator>("env", "set_output_data");
        rhf_t::add<callbacks, &callbacks::query_database, wasm_allocator>("env", "query_database");
        rhf_t::add<callbacks, &callbacks::print_range, wasm_allocator>("env", "print_range");
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
