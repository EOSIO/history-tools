// copyright defined in LICENSE.txt

// todo: timeout wasm
// todo: timeout sql
// todo: what should memory size limit be?
// todo: check callbacks for recursion to limit stack size
// todo: make sure spidermonkey limits stack size
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
#include "wasm_interface.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http/vector_body.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/signals2/connection.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/datastream.hpp>
#include <fc/scoped_exit.hpp>
#include <fstream>

using namespace abieos;
using namespace appbase;
using namespace std::literals;
using namespace wasm;
namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace ws    = boost::beast::websocket;
using tcp       = asio::ip::tcp;

struct state : wasm_state {
    std::string                         allow_origin  = {};
    std::string                         wasm_dir      = {};
    std::string                         js_dir        = {};
    std::shared_ptr<database_interface> db_iface      = {};
    std::unique_ptr<::query_session>    query_session = {};
    state_history::fill_status          fill_status   = {};

    static state& from_context(JSContext* cx) { return *reinterpret_cast<state*>(JS_GetContextPrivate(cx)); }
};

// args: wasm_name
static bool get_wasm(JSContext* cx, unsigned argc, JS::Value* vp) {
    auto&        state = ::state::from_context(cx);
    JS::CallArgs args  = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "get_wasm", 1))
        return false;
    std::string wasm_name;
    if (!js_assert(args[0].isString() && convert_str(cx, args[0].toString(), wasm_name), cx, "get_wasm: invalid args"))
        return false;

    // todo: sanitize wasm_name
    wasm_name = state.wasm_dir + "/" + wasm_name + "-server.wasm";

    try {
        std::fstream file(wasm_name, std::ios_base::in | std::ios_base::binary);
        file.seekg(0, std::ios_base::end);
        auto len = file.tellg();
        if (len <= 0)
            return js_assert(false, cx, ("can not read " + wasm_name).c_str());
        file.seekg(0, std::ios_base::beg);
        auto data = malloc(len);
        if (!data)
            return js_assert(false, cx, ("out of memory reading " + wasm_name).c_str());
        file.read(reinterpret_cast<char*>(data), len);
        JS::CallArgs args = CallArgsFromVp(argc, vp);
        args.rval().setObjectOrNull(JS_NewArrayBufferWithContents(cx, len, data));
        return true;
    } catch (...) {
        return js_assert(false, cx, ("error reading " + wasm_name).c_str());
    }
}

// args: ArrayBuffer, row_request_begin, row_request_end, callback
static bool query_database(JSContext* cx, unsigned argc, JS::Value* vp) {
    auto&        state = ::state::from_context(cx);
    JS::CallArgs args  = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "query_database", 4))
        return false;
    std::vector<char> args_bin;
    bool              ok = true;
    {
        JS::AutoCheckCannotGC checkGC;
        auto                  b = get_input_buffer(args, 0, 1, 2, checkGC);
        if (b.pos) {
            try {
                args_bin.assign(b.pos, b.end);
            } catch (...) {
                ok = false;
            }
        }
    }
    if (!ok)
        return js_assert(false, cx, "query_database: invalid args");

    try {
        auto result = state.query_session->query_database({args_bin.data(), args_bin.data() + args_bin.size()}, state.fill_status.head);
        auto data   = get_mem_from_callback(cx, args, 3, result.size());
        if (!js_assert(data, cx, "query_database: failed to fetch buffer from callback"))
            return false;
        memcpy(data, result.data(), result.size());
        return true;
    } catch (const std::exception& e) {
        return js_assert(false, cx, ("query_database: "s + e.what()).c_str());
    } catch (...) {
        return js_assert(false, cx, "query_database error");
    }
} // query_database

static const JSFunctionSpec functions[] = {
    JS_FN("get_database_status", get_database_status, 0, 0), //
    JS_FN("get_input_data", get_input_data, 0, 0),           //
    JS_FN("get_wasm", get_wasm, 0, 0),                       //
    JS_FN("print_js_str", print_js_str, 0, 0),               //
    JS_FN("print_wasm_str", print_wasm_str, 0, 0),           //
    JS_FN("query_database", query_database, 0, 0),           //
    JS_FN("set_output_data", set_output_data, 0, 0),         //
    JS_FS_END                                                //
};

static void init_glue(::state& state) {
    create_global(state, functions);
    std::string js_name{state.js_dir + "/" + "glue.js"};
    execute(state, "glue.js", read_string(js_name.c_str()));
}

static void fill_context_data(::state& state) {
    state.database_status.clear();
    abieos::native_to_bin(state.database_status, state.fill_status.head);
    abieos::native_to_bin(state.database_status, state.fill_status.head_id);
    abieos::native_to_bin(state.database_status, state.fill_status.irreversible);
    abieos::native_to_bin(state.database_status, state.fill_status.irreversible_id);
    abieos::native_to_bin(state.database_status, state.fill_status.first);
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

            JSAutoRealm           realm(state.context.cx, state.global);
            JS::RootedValue       rval(state.context.cx);
            JS::AutoValueArray<1> args(state.context.cx);
            args[0].set(JS::StringValue(JS_NewStringCopyZ(state.context.cx, ((std::string)wasm_name).c_str())));
            if (!JS_CallFunctionName(state.context.cx, state.global, "run_query", args, &rval)) {
                // todo: detect assert
                JS_ClearPendingException(state.context.cx);
                throw std::runtime_error("JS_CallFunctionName failed");
            }

            if (did_fork(state))
                return false;

            push_varuint32(result, state.reply.size());
            result.insert(result.end(), state.reply.begin(), state.reply.end());
        }
        return true;
    });
    return result;
}

static const std::vector<char>& legacy_query(::state& state, const std::string& target, const std::vector<char>& request) {
    std::vector<char> req;
    abieos::native_to_bin(req, target);
    abieos::native_to_bin(req, request);
    state.request = input_buffer{req.data(), req.data() + req.size()};
    retry_loop(state, [&]() {
        JSAutoRealm           realm(state.context.cx, state.global);
        JS::RootedValue       rval(state.context.cx);
        JS::AutoValueArray<1> args(state.context.cx);
        args[0].set(JS::StringValue(JS_NewStringCopyZ(state.context.cx, "legacy")));
        if (!JS_CallFunctionName(state.context.cx, state.global, "run_query", args, &rval)) {
            // todo: detect assert
            JS_ClearPendingException(state.context.cx);
            throw std::runtime_error("JS_CallFunctionName failed");
        }
        return !did_fork(state);
    });
    return state.reply;
}

static void fail(beast::error_code ec, char const* what) { elog("${w}: ${s}", ("w", what)("s", ec.message())); }

static void handle_request(::state& state, tcp::socket& socket, http::request<http::vector_body<char>> req, beast::error_code& ec) {
    auto const error = [&req](http::status status, beast::string_view why) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = why.to_string();
        res.prepare_payload();
        return res;
    };

    auto const ok = [&state, &req](std::vector<char> reply, const char* content_type) {
        http::response<http::vector_body<char>> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, content_type);
        if (state.allow_origin != "*")
            res.set(http::field::access_control_allow_origin, state.allow_origin);
        res.keep_alive(req.keep_alive());
        res.body() = std::move(reply);
        res.prepare_payload();
        return res;
    };

    auto send = [&](const auto& msg) {
        http::serializer<false, typename std::decay_t<decltype(msg)>::body_type> sr{msg};
        http::write(socket, sr, ec);
    };

    auto target = req.target();
    try {
        if (target == "/wasmql/v1/query") {
            if (req.method() != http::verb::post)
                return send(error(http::status::bad_request, "Unsupported HTTP-method\n"));
            return send(ok(query(state, req.body()), "application/octet-stream"));
        } else if (target.starts_with("/v1/")) {
            if (req.method() != http::verb::post)
                return send(error(http::status::bad_request, "Unsupported HTTP-method\n"));
            return send(ok(legacy_query(state, std::string(target.begin(), target.end()), req.body()), "application/octet-stream"));
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

wasm_ql_plugin::~wasm_ql_plugin() { ilog("wasm_ql_plugin stopped"); }

void wasm_ql_plugin::set_program_options(options_description& cli, options_description& cfg) {
    auto op = cfg.add_options();
    op("wql-listen", bpo::value<std::string>()->default_value("localhost:8880"), "Endpoint to listen on");
    op("wql-allow-origin", bpo::value<std::string>()->default_value("*"), "Access-Control-Allow-Origin header. Use \"*\" to allow any.");
    op("wql-wasm-dir", bpo::value<std::string>()->default_value("."), "Directory to fetch WASMs from");
    op("wql-js-dir", bpo::value<std::string>()->default_value("../src"), "Directory to fetch JSs from");
    op("wql-console", "Show console output");
}

void wasm_ql_plugin::plugin_initialize(const variables_map& options) {
    try {
        JS_Init();
        auto ip_port            = options.at("wql-listen").as<std::string>();
        my->state               = std::make_unique<::state>();
        my->state->console      = options.count("wql-console");
        my->endpoint_port       = ip_port.substr(ip_port.find(':') + 1, ip_port.size());
        my->endpoint_address    = ip_port.substr(0, ip_port.find(':'));
        my->state->allow_origin = options.at("wql-allow-origin").as<std::string>();
        my->state->wasm_dir     = options.at("wql-wasm-dir").as<std::string>();
        my->state->js_dir       = options.at("wql-js-dir").as<std::string>();
        init_glue(*my->state);
    }
    FC_LOG_AND_RETHROW()
}

void wasm_ql_plugin::plugin_startup() {
    if (!my->state->db_iface)
        throw std::runtime_error("wasm_ql_plugin needs either wasm_ql_pg_plugin or wasm_ql_lmdb_plugin");
    my->listen();
}
void wasm_ql_plugin::plugin_shutdown() { my->stopping = true; }

void wasm_ql_plugin::set_database(std::shared_ptr<database_interface> db_iface) { my->state->db_iface = std::move(db_iface); }
