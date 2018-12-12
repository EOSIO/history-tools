// copyright defined in LICENSE.txt

// todo: balance history
// todo: timeout wasm
// todo: timeout sql
// todo: what should memory size limit be?
// todo: check callbacks for recursion to limit stack size
// todo: make sure spidermonkey limits stack size
// todo: global constructors in wasm
// todo: don't allow queries past head
// todo: kill a wasm execution if a fork change happens
//       for now: warn about having multiple queries past irreversible
// todo: cap max_results

#define DEBUG

#include "queries.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http/vector_body.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <variant>

#include "jsapi.h"

#include "js/AutoByteString.h"
#include "js/CompilationAndEvaluation.h"
#include "js/Initialization.h"
#include "jsfriendapi.h"

using namespace abieos;
using namespace std::literals;
using namespace sql_conversion;
namespace bpo   = boost::program_options;
namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
using tcp       = asio::ip::tcp;

using std::string;

string readStr(const char* filename) {
    try {
        std::fstream file(filename, std::ios_base::in | std::ios_base::binary);
        file.seekg(0, std::ios_base::end);
        auto len = file.tellg();
        file.seekg(0, std::ios_base::beg);
        string result(len, 0);
        file.read(result.data(), len);
        return result;
    } catch (const std::exception& e) {
        throw std::runtime_error("Error reading "s + filename);
    }
}

bool buildIdOp(JS::BuildIdCharVector* buildId) {
    // todo: causes linker errors
    // const char id[] = "something";
    // return buildId->append(id, sizeof(id));
    return true;
}

static JSClassOps global_ops = {
    nullptr,                  // addProperty
    nullptr,                  // delProperty
    nullptr,                  // enumerate
    nullptr,                  // newEnumerate
    nullptr,                  // resolve
    nullptr,                  // mayResolve
    nullptr,                  // finalize
    nullptr,                  // call
    nullptr,                  // hasInstance
    nullptr,                  // construct
    JS_GlobalObjectTraceHook, // trace
};

static JSClass global_class = {
    "global",
    JSCLASS_GLOBAL_FLAGS,
    &global_ops,
    {},
};

struct ContextWrapper {
    JSContext* cx;

    ContextWrapper() {
        cx = JS_NewContext(8L * 1024 * 1024);
        if (!cx)
            throw std::runtime_error("JS_NewContext failed");
        if (!JS::InitSelfHostedCode(cx)) {
            JS_DestroyContext(cx);
            throw std::runtime_error("JS::InitSelfHostedCode failed");
        }
        JS::SetBuildIdOp(cx, buildIdOp);
    }

    ~ContextWrapper() { JS_DestroyContext(cx); }
};

struct foo {
    ContextWrapper       context;
    JS::RootedObject     global;
    asio::io_context     ioc;
    query_config::config config;
    string               schema;
    pqxx::connection     sql_connection;
    std::vector<char>    request;
    std::vector<char>    reply;

    foo()
        : global(context.cx) {}
};

std::unique_ptr<foo> foo_global; // todo: store in JS context instead of global variable

bool convert_str(JSContext* cx, JSString* str, string& dest) {
    auto len = JS_GetStringEncodingLength(cx, str);
    if (len == size_t(-1))
        return false;
    dest.clear();
    dest.resize(len);
    return JS_EncodeStringToBuffer(cx, str, dest.data(), len);
}

bool print_js_str(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    string       s;
    for (unsigned i = 0; i < args.length(); ++i)
        if (args[i].isString() && convert_str(cx, args[i].toString(), s))
            std::cerr << s;
    return true;
}

bool js_assert(bool cond, JSContext* cx, const char* s) {
    if (!cond)
        JS_ReportErrorUTF8(cx, "%s", s);
    return cond;
}

input_buffer get_input_buffer(JS::CallArgs& args, unsigned buffer_arg, unsigned begin_arg, unsigned end_arg, JS::AutoRequireNoGC& nogc) {
    if (!args[buffer_arg].isObject() || !args[begin_arg].isInt32() || !args[end_arg].isInt32())
        return {};
    JSObject* memory = &args[buffer_arg].toObject();
    auto      begin  = args[begin_arg].toInt32();
    auto      end    = args[end_arg].toInt32();
    if (!JS_IsArrayBufferObject(memory) || !JS_ArrayBufferHasData(memory) || begin < 0 || end < 0 || begin > end)
        return {};
    auto  buf_len = JS_GetArrayBufferByteLength(memory);
    bool  dummy   = false;
    auto* data    = reinterpret_cast<const char*>(JS_GetArrayBufferData(memory, &dummy, nogc));
    if (!data || uint64_t(end) > buf_len)
        return {};
    return {data + begin, data + end};
}

char* get_mem_from_callback(JSContext* cx, JS::CallArgs& args, unsigned callback_arg, int32_t size) {
    JS::RootedValue       cb_result(cx);
    JS::AutoValueArray<1> cb_args(cx);
    cb_args[0].set(JS::NumberValue(size));
    if (!JS_CallFunctionValue(cx, nullptr, args[callback_arg], cb_args, &cb_result) || !cb_result.isObject())
        return nullptr;

    JS::RootedObject cb_result_obj(cx, &cb_result.toObject());
    JS::RootedValue  cb_result_buf(cx);
    JS::RootedValue  cb_result_size(cx);
    if (!JS_GetElement(cx, cb_result_obj, 0, &cb_result_buf) || !JS_GetElement(cx, cb_result_obj, 1, &cb_result_size) ||
        !cb_result_buf.isObject() || !cb_result_size.isInt32())
        return nullptr;

    JS::RootedObject memory(cx, &cb_result_buf.toObject());
    auto             begin = cb_result_size.toInt32();
    auto             end   = uint64_t(begin) + uint64_t(size);
    if (!JS_IsArrayBufferObject(memory) || !JS_ArrayBufferHasData(memory) || begin < 0)
        return nullptr;
    auto buf_len = JS_GetArrayBufferByteLength(memory);
    bool dummy   = false;

    JS::AutoCheckCannotGC checkGC;
    auto                  data = reinterpret_cast<char*>(JS_GetArrayBufferData(memory, &dummy, checkGC));
    if (!data || end > buf_len)
        return nullptr;
    return data + begin;
}

bool print_wasm_str(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "print_wasm_str", 3))
        return false;
    {
        JS::AutoCheckCannotGC checkGC;
        auto                  buf = get_input_buffer(args, 0, 1, 2, checkGC);
        if (buf.pos) {
            std::cerr.write(buf.pos, buf.end - buf.pos);
            return true;
        }
    }
    return js_assert(false, cx, "print_wasm_str: invalid args");
}

bool get_wasm(JSContext* cx, unsigned argc, JS::Value* vp) {
    try {
        std::fstream file("test.wasm", std::ios_base::in | std::ios_base::binary);
        file.seekg(0, std::ios_base::end);
        auto len = file.tellg();
        file.seekg(0, std::ios_base::beg);
        auto data = malloc(len);
        if (!data) {
            std::cerr << "!!!!! a\n";
            JS_ReportOutOfMemory(cx);
            return false;
        }
        file.read(reinterpret_cast<char*>(data), len);
        JS::CallArgs args = CallArgsFromVp(argc, vp);
        args.rval().setObjectOrNull(JS_NewArrayBufferWithContents(cx, len, data));
        return true;
    } catch (...) {
        std::cerr << "!!!!! b\n";
        JS_ReportOutOfMemory(cx);
        return false;
    }
}

// args: callback
bool get_request(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "get_request", 1))
        return false;
    try {
        if (!js_assert((uint32_t)foo_global->request.size() == foo_global->request.size(), cx, "get_request: request is too big"))
            return false;
        auto data = get_mem_from_callback(cx, args, 0, foo_global->request.size());
        if (!js_assert(data, cx, "get_request: failed to fetch buffer from callback"))
            return false;
        memcpy(data, foo_global->request.data(), foo_global->request.size());
        return true;
    } catch (const std::exception& e) {
        std::cerr << "!!!!! c: " << e.what() << "\n";
        JS_ReportOutOfMemory(cx);
        return false;
    } catch (...) {
        std::cerr << "!!!!! c\n";
        JS_ReportOutOfMemory(cx);
        return false;
    }
} // get_request

// args: ArrayBuffer, row_request_begin, row_request_end
bool set_reply(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "set_reply", 3))
        return false;
    bool ok = true;
    {
        JS::AutoCheckCannotGC checkGC;
        auto                  b = get_input_buffer(args, 0, 1, 2, checkGC);
        if (b.pos) {
            try {
                foo_global->reply.assign(b.pos, b.end);
            } catch (...) {
                ok = false;
            }
        }
    }
    return js_assert(ok, cx, "set_reply: invalid args");
}

// args: ArrayBuffer, row_request_begin, row_request_end, callback
bool exec_query(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "exec_query", 4))
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
        return js_assert(false, cx, "exec_query: invalid args");

    try {
        abieos::input_buffer args_buf{args_bin.data(), args_bin.data() + args_bin.size()};
        abieos::name         query_name;
        if (!abieos::bin_to_native(query_name, args_buf))
            return js_assert(false, cx, "exec_query: invalid args");

        auto it = foo_global->config.query_map.find(query_name);
        if (it == foo_global->config.query_map.end())
            return js_assert(false, cx, ("exec_query: unknown query: " + (string)query_name).c_str());
        query_config::query& query = *it->second;
        query_config::table& table = *query.result_table;

        std::string query_str = "select * from \"" + foo_global->schema + "\"." + query.function + "(";
        query_str += sql_conversion::sql_type_for<uint32_t>.bin_to_sql(args_buf); // max_block_index
        for (auto& type : query.types)
            query_str += sep + type.bin_to_sql(args_buf);
        for (auto& type : query.types)
            query_str += sep + type.bin_to_sql(args_buf);
        query_str += sep + sql_conversion::sql_type_for<uint32_t>.bin_to_sql(args_buf); // max_block_index
        query_str += ")";

        pqxx::work        t(foo_global->sql_connection);
        auto              exec_result = t.exec(query_str);
        std::vector<char> result_bin;
        std::vector<char> row_bin;
        push_varuint32(result_bin, exec_result.size());
        for (const auto& r : exec_result) {
            row_bin.clear();
            int i = 0;
            for (auto& type : table.types)
                type.sql_to_bin(row_bin, r[i++]);
            if (!js_assert((uint32_t)row_bin.size() == row_bin.size(), cx, "exec_query: row is too big"))
                return false;
            push_varuint32(result_bin, row_bin.size());
            result_bin.insert(result_bin.end(), row_bin.begin(), row_bin.end());
        }
        t.commit();
        if (!js_assert((uint32_t)result_bin.size() == result_bin.size(), cx, "exec_query: result is too big"))
            return false;
        auto data = get_mem_from_callback(cx, args, 3, result_bin.size());
        if (!js_assert(data, cx, "exec_query: failed to fetch buffer from callback"))
            return false;
        memcpy(data, result_bin.data(), result_bin.size());
        return true;
    } catch (const std::exception& e) {
        std::cerr << "!!!!! c: " << e.what() << "\n";
        JS_ReportOutOfMemory(cx);
        return false;
    } catch (...) {
        std::cerr << "!!!!! c\n";
        JS_ReportOutOfMemory(cx);
        return false;
    }
} // exec_query

static const JSFunctionSpec functions[] = {
    JS_FN("exec_query", exec_query, 0, 0),         //
    JS_FN("get_request", get_request, 0, 0),       //
    JS_FN("get_wasm", get_wasm, 0, 0),             //
    JS_FN("print_js_str", print_js_str, 0, 0),     //
    JS_FN("print_wasm_str", print_wasm_str, 0, 0), //
    JS_FN("set_reply", set_reply, 0, 0),           //
    JS_FS_END                                      //
};

void init_glue() {
    JSAutoRequest req(foo_global->context.cx);

    JS::RealmOptions options;
    foo_global->global.set(JS_NewGlobalObject(foo_global->context.cx, &global_class, nullptr, JS::FireOnNewGlobalHook, options));
    if (!foo_global->global)
        throw std::runtime_error("JS_NewGlobalObject failed");

    JSAutoRealm realm(foo_global->context.cx, foo_global->global);
    if (!JS::InitRealmStandardClasses(foo_global->context.cx))
        throw std::runtime_error("JS::InitRealmStandardClasses failed");
    if (!JS_DefineFunctions(foo_global->context.cx, foo_global->global, functions))
        throw std::runtime_error("JS_DefineFunctions failed");
    if (!JS_DefineProperty(foo_global->context.cx, foo_global->global, "global", foo_global->global, 0))
        throw std::runtime_error("JS_DefineProperty failed");

    auto               script   = readStr("../src/glue.js");
    const char*        filename = "noname";
    int                lineno   = 1;
    JS::CompileOptions opts(foo_global->context.cx);
    opts.setFileAndLine(filename, lineno);
    JS::RootedValue rval(foo_global->context.cx);
    bool            ok = JS::Evaluate(foo_global->context.cx, opts, script.c_str(), script.size(), &rval);
    if (!ok)
        throw std::runtime_error("JS::Evaluate failed");
}

void run() {
    JSAutoRequest req(foo_global->context.cx);
    JSAutoRealm   realm(foo_global->context.cx, foo_global->global);

    JS::RootedValue       rval(foo_global->context.cx);
    JS::AutoValueArray<1> args(foo_global->context.cx);
    args[0].set(JS::NumberValue(1234));
    if (!JS_CallFunctionName(foo_global->context.cx, foo_global->global, "run", args, &rval)) {
        JS_ClearPendingException(foo_global->context.cx);
        throw std::runtime_error("JS_CallFunctionName failed");
    }
}

void fail(beast::error_code ec, char const* what) { std::cerr << what << ": " << ec.message() << "\n"; }

void handle_request(tcp::socket& socket, http::request<http::vector_body<char>> req, beast::error_code& ec) {
    auto const bad_request = [&req](beast::string_view why) {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = why.to_string();
        res.prepare_payload();
        return res;
    };

    auto const not_found = [&req](beast::string_view target) {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + target.to_string() + "' was not found.\n";
        res.prepare_payload();
        return res;
    };

    auto const server_error = [&req](beast::string_view what) {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = what.to_string() + "\n";
        res.prepare_payload();
        return res;
    };

    auto const ok = [&req](std::vector<char> reply) {
        http::response<http::vector_body<char>> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "application/octet-stream");
        res.keep_alive(req.keep_alive());
        res.body() = std::move(reply);
        res.prepare_payload();
        return res;
    };

    auto send = [&](const auto& msg) {
        http::serializer<false, typename std::decay_t<decltype(msg)>::body_type> sr{msg};
        http::write(socket, sr, ec);
    };

    if (req.target() == "/wasmql/v1/query") {
        if (req.method() != http::verb::post)
            return send(bad_request("Unsupported HTTP-method\n"));
        foo_global->request = std::move(req.body());
        try {
            run();
            return send(ok(std::move(foo_global->reply)));
        } catch (...) {
            return send(server_error("wasm execution failed"));
        }
    }

    return send(not_found(req.target()));
}

void accepted(tcp::socket socket) {
    beast::error_code ec;

    beast::flat_buffer buffer;
    for (;;) {
        http::request<http::vector_body<char>> req;
        http::read(socket, buffer, req, ec);
        if (ec == http::error::end_of_stream)
            break;
        if (ec)
            return fail(ec, "read");

        handle_request(socket, std::move(req), ec);
        if (ec)
            return fail(ec, "write");
    }
    socket.shutdown(tcp::socket::shutdown_send, ec);
}

int main(int argc, const char* argv[]) {
    std::cerr << std::unitbuf;

    try {
        bpo::options_description desc{"Options"};
        auto                     op = desc.add_options();
        op("help,h", "Help screen");
        op("schema,s", bpo::value<string>()->default_value("chain"), "Database schema");
        op("query-config,q", bpo::value<string>()->default_value("../src/query-config.json"), "Query configuration");
        op("address,a", bpo::value<string>()->default_value("localhost"), "Address to listen on");
        op("port,p", bpo::value<string>()->default_value("8080"), "Port to listen on)");
        bpo::variables_map vm;
        bpo::store(bpo::parse_command_line(argc, argv, desc), vm);
        bpo::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << '\n';
            return 0;
        }

        JS_Init();
        foo_global         = std::make_unique<foo>();
        foo_global->schema = vm["schema"].as<string>();

        auto x = readStr(vm["query-config"].as<string>().c_str());
        if (!json_to_native(foo_global->config, x))
            throw std::runtime_error("error processing " + vm["query-config"].as<string>());
        foo_global->config.prepare();

        init_glue();

        tcp::resolver resolver(foo_global->ioc);
        auto          addr = resolver.resolve(tcp::resolver::query(tcp::v4(), vm["address"].as<string>(), vm["port"].as<string>()));
        tcp::acceptor acceptor{foo_global->ioc, *addr.begin()};
        std::cerr << "listening on " << vm["address"].as<string>() << ":" << vm["port"].as<string>() << "\n";

        for (;;) {
            try {
                tcp::socket socket{foo_global->ioc};
                acceptor.accept(socket);
                std::cerr << "accepted\n";
                accepted(std::move(socket));
            } catch (const std::exception& e) {
                std::cerr << "error: " << e.what() << "\n";
            } catch (...) {
                std::cerr << "error: unknown exception\n";
            }
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "error: unknown exception\n";
        return 1;
    }
}
