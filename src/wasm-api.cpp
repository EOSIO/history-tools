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
// todo: embed each row in bytes to allow binary extensions

#define DEBUG

#include "queries.hpp"

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
namespace bpo = boost::program_options;

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

struct foo {
    query_config::config config;
    string               schema;
    pqxx::connection     sql_connection;
};

std::unique_ptr<foo> foo_global; // todo: store in JS context instead of global variable

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
        push_varuint32(result_bin, exec_result.size());
        for (const auto& r : exec_result) {
            int i = 0;
            for (auto& type : table.types)
                type.sql_to_bin(result_bin, r[i++]);
        }
        t.commit();
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
    JS_FN("print_js_str", print_js_str, 0, 0),     //
    JS_FN("print_wasm_str", print_wasm_str, 0, 0), //
    JS_FN("get_wasm", get_wasm, 0, 0),             //
    JS_FN("exec_query", exec_query, 0, 0),         //
    JS_FS_END                                      //
};

void runit() {
    JS_Init();
    {
        ContextWrapper context;
        JSAutoRequest  ar(context.cx);

        JS::RealmOptions options;
        JS::RootedObject global(context.cx, JS_NewGlobalObject(context.cx, &global_class, nullptr, JS::FireOnNewGlobalHook, options));
        if (!global)
            throw std::runtime_error("JS_NewGlobalObject failed");

        {
            JSAutoRealm ar(context.cx, global);
            if (!JS::InitRealmStandardClasses(context.cx))
                throw std::runtime_error("JS::InitRealmStandardClasses failed");
            if (!JS_DefineFunctions(context.cx, global, functions))
                throw std::runtime_error("JS_DefineFunctions failed");

            auto               script   = readStr("../src/glue.js");
            const char*        filename = "noname";
            int                lineno   = 1;
            JS::CompileOptions opts(context.cx);
            opts.setFileAndLine(filename, lineno);
            JS::RootedValue rval(context.cx);
            bool            ok = JS::Evaluate(context.cx, opts, script.c_str(), script.size(), &rval);
            if (!ok)
                throw std::runtime_error("JS::Evaluate failed");
        }
    }
    JS_ShutDown();
}

int main(int argc, const char* argv[]) {
    std::cerr << std::unitbuf;

    try {
        bpo::options_description desc{"Options"};
        auto                     op = desc.add_options();
        op("help,h", "Help screen");
        op("schema,s", bpo::value<string>()->default_value("chain"), "Database schema");
        op("query-config,q", bpo::value<string>()->default_value("../src/query-config.json"), "Query configuration");
        bpo::variables_map vm;
        bpo::store(bpo::parse_command_line(argc, argv, desc), vm);
        bpo::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << '\n';
            return 0;
        }

        foo_global         = std::make_unique<foo>();
        foo_global->schema = vm["schema"].as<string>();

        auto x = readStr(vm["query-config"].as<string>().c_str());
        if (!json_to_native(foo_global->config, x))
            throw std::runtime_error("error processing " + vm["query-config"].as<string>());
        foo_global->config.prepare();

        runit();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
