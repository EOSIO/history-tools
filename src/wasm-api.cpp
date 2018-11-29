// copyright defined in LICENSE.txt

// todo: timeout wasm
// todo: timeout sql
// todo: what should memory size limit be?
// todo: check callbacks for recursion to limit stack size
// todo: make sure spidermonkey limits stack size
// todo: global constructors in wasm

#define DEBUG

#include "abieos.hpp"

#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <pqxx/pqxx>
#include <sstream>
#include <string>

#include "jsapi.h"

#include "js/AutoByteString.h"
#include "js/CompilationAndEvaluation.h"
#include "js/Initialization.h"
#include "jsfriendapi.h"

using namespace abieos;

namespace bpo = boost::program_options;

template <class C, typename M>
const C* class_from_void(M C::*, const void* v) {
    return reinterpret_cast<const C*>(v);
}

template <auto P>
auto& member_from_void(const member_ptr<P>&, const void* p) {
    return class_from_void(P, p)->*P;
}

template <typename T>
void native_to_bin(std::vector<char>& bin, const T& obj);
template <typename T>
void native_to_bin(std::vector<char>& bin, const std::vector<T>& obj);
void native_to_bin(std::vector<char>& bin, const std::string& obj);

template <typename T>
void native_to_bin(std::vector<char>& bin, const T& obj) {
    if constexpr (std::is_trivially_copyable_v<T>) { // todo: bad condition
        push_raw(bin, obj);
    } else {
        for_each_field((T*)nullptr, [&](auto* name, auto member_ptr) { //
            native_to_bin(bin, member_from_void(member_ptr, &obj));
        });
    }
}

template <typename T>
void native_to_bin(std::vector<char>& bin, const std::vector<T>& obj) {
    push_varuint32(bin, obj.size());
    for (auto& v : obj) {
        native_to_bin(bin, v);
    }
}

void native_to_bin(std::vector<char>& bin, const std::string& obj) {
    push_varuint32(bin, obj.size());
    bin.insert(bin.end(), obj.begin(), obj.end());
}

std::string readStr(const char* filename) {
    std::fstream file(filename, std::ios_base::in | std::ios_base::binary);
    file.seekg(0, std::ios_base::end);
    auto len = file.tellg();
    file.seekg(0, std::ios_base::beg);
    std::string result(len, 0);
    file.read(result.data(), len);
    return result;
}

bool buildIdOp(JS::BuildIdCharVector* buildId) {
    // todo: causes linker errors
    // const char id[] = "something";
    // return buildId->append(id, sizeof(id));
    return true;
}

struct foo {
    std::string      schema;
    pqxx::connection sql_connection;
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

bool convert_str(JSContext* cx, JSString* str, std::string& dest) {
    auto len = JS_GetStringEncodingLength(cx, str);
    if (len == size_t(-1))
        return false;
    dest.clear();
    dest.resize(len);
    return JS_EncodeStringToBuffer(cx, str, dest.data(), len);
}

bool printStr(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    std::string  s;
    for (unsigned i = 0; i < args.length(); ++i)
        if (args[i].isString() && convert_str(cx, args[i].toString(), s))
            std::cerr << s << "\n";
    return true;
}

bool printi32(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    std::string  s;
    for (unsigned i = 0; i < args.length(); ++i)
        if (args[i].isInt32())
            std::cerr << args[i].toInt32();
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

bool printWasmStr(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "printWasmStr", 3))
        return false;
    {
        JS::AutoCheckCannotGC checkGC;
        auto                  buf = get_input_buffer(args, 0, 1, 2, checkGC);
        if (buf.pos) {
            std::cerr.write(buf.pos, buf.end - buf.pos);
            return true;
        }
    }
    return js_assert(false, cx, "printWasmStr: invalid args");
}

bool readWasm(JSContext* cx, unsigned argc, JS::Value* vp) {
    try {
        std::fstream file("test.wasm", std::ios_base::in | std::ios_base::binary);
        file.seekg(0, std::ios_base::end);
        auto len = file.tellg();
        file.seekg(0, std::ios_base::beg);
        auto data = malloc(len);
        if (!data) {
            std::cerr << "!!!!!\n";
            JS_ReportOutOfMemory(cx);
            return false;
        }
        file.read(reinterpret_cast<char*>(data), len);
        JS::CallArgs args = CallArgsFromVp(argc, vp);
        args.rval().setObjectOrNull(JS_NewArrayBufferWithContents(cx, len, data));
        return true;
    } catch (...) {
        std::cerr << "!!!!!\n";
        JS_ReportOutOfMemory(cx);
        return false;
    }
}

struct db_result {
    uint32_t    block_index = 0;
    bool        present     = false;
    name        code;
    name        table;
    name        scope;
    uint64_t    primary_key = 0;
    name        payer;
    std::string value;
};

template <typename F>
constexpr void for_each_field(db_result*, F f) {
    f("block_index", member_ptr<&db_result::block_index>{});
    f("present", member_ptr<&db_result::present>{});
    f("code", member_ptr<&db_result::code>{});
    f("table", member_ptr<&db_result::table>{});
    f("scope", member_ptr<&db_result::scope>{});
    f("primary_key", member_ptr<&db_result::primary_key>{});
    f("payer", member_ptr<&db_result::payer>{});
    f("value", member_ptr<&db_result::value>{});
}

bool testdb(JSContext* cx, unsigned argc, JS::Value* vp) {
    try {
        JS::CallArgs args = CallArgsFromVp(argc, vp);
        if (!args.requireAtLeast(cx, "testdb", 1))
            return false;
        pqxx::work t(foo_global->sql_connection);
        auto       result = t.exec(R"(
            select
                distinct on(code, "table", scope, primary_key)
                block_index, present, code, "table", scope, primary_key, payer, value
            from
                chain.contract_row
            where
                block_index <= 30000000
                and code = 'eosio.token'
                and "table" = 'accounts'
                and scope > 'z'
                and primary_key = 5459781
            order by
                code,
                "table",
                primary_key,
                scope,
                block_index desc,
                present desc
            limit 10
        )");

        std::vector<db_result> v;
        for (const auto& r : result) {
            v.push_back(db_result{
                .block_index = r[0].as<uint32_t>(),
                .present     = r[1].as<bool>(),
                .code        = name{r[2].as<std::string>().c_str()},
                .table       = name{r[3].as<std::string>().c_str()},
                .scope       = name{r[4].as<std::string>().c_str()},
                .primary_key = r[5].as<uint64_t>(),
                .payer       = name{r[6].as<std::string>().c_str()},
                .value       = r[7].as<std::string>(),
            });
        }
        t.commit();

        std::vector<char> bin;
        native_to_bin(bin, v);
        auto data = get_mem_from_callback(cx, args, 0, bin.size());
        if (!js_assert(data, cx, "testdb: failed to fetch buffer from callback"))
            return false;
        memcpy(data, bin.data(), bin.size());
        return true;
    } catch (...) {
        std::cerr << "!!!!!\n";
        JS_ReportOutOfMemory(cx);
        return false;
    }
} // testdb

static const JSFunctionSpec functions[] = {
    JS_FN("printStr", printStr, 0, 0),         //
    JS_FN("printi32", printi32, 0, 0),         //
    JS_FN("printWasmStr", printWasmStr, 0, 0), //
    JS_FN("readWasm", readWasm, 0, 0),         //
    JS_FN("testdb", testdb, 0, 0),             //
    JS_FS_END                                  //
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
        op("schema,s", bpo::value<std::string>()->default_value("chain"), "Database schema");
        bpo::variables_map vm;
        bpo::store(bpo::parse_command_line(argc, argv, desc), vm);
        bpo::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << '\n';
            return 0;
        }

        foo_global         = std::make_unique<foo>();
        foo_global->schema = vm["schema"].as<std::string>();
        runit();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
