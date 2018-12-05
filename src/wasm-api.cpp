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
#include <variant>

#include "jsapi.h"

#include "js/AutoByteString.h"
#include "js/CompilationAndEvaluation.h"
#include "js/Initialization.h"
#include "jsfriendapi.h"

using namespace abieos;
using namespace std::literals;
namespace bpo = boost::program_options;

using std::string;
using std::to_string;

static const string null_value = "null";
static const string sep        = ",";

inline string quote(string s) { return "'" + s + "'"; }
inline string quote_bytea(string s) { return "'\\x" + s + "'"; }

// clang-format off
inline string sql_str(bool v)               { return v ? "true" : "false";}
inline string sql_str(uint16_t v)           { return to_string(v); }
inline string sql_str(int16_t v)            { return to_string(v); }
inline string sql_str(uint32_t v)           { return to_string(v); }
inline string sql_str(int32_t v)            { return to_string(v); }
inline string sql_str(uint64_t v)           { return to_string(v); }
inline string sql_str(int64_t v)            { return to_string(v); }
inline string sql_str(varuint32 v)          { return string(v); }
inline string sql_str(varint32 v)           { return string(v); }
inline string sql_str(int128 v)             { return string(v); }
inline string sql_str(uint128 v)            { return string(v); }
inline string sql_str(float128 v)           { return quote_bytea(string(v)); }
inline string sql_str(name v)               { return quote(v.value ? string(v) : ""s); }
inline string sql_str(time_point v)         { return v.microseconds ? quote(string(v)): null_value; }
inline string sql_str(time_point_sec v)     { return v.utc_seconds ? quote(string(v)): null_value; }
inline string sql_str(block_timestamp v)    { return v.slot ?  quote(string(v)) : null_value; }
inline string sql_str(checksum256 v)        { return quote(v.value == checksum256{}.value ? "" : string(v)); }
inline string sql_str(const public_key& v)  { return quote(public_key_to_string(v)); }
// clang-format on

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

inline void native_to_bin(std::vector<char>& bin, const name& obj) {
    native_to_bin(bin, obj.value); //
}

void native_to_bin(std::vector<char>& bin, const string& obj) {
    push_varuint32(bin, obj.size());
    bin.insert(bin.end(), obj.begin(), obj.end());
}

void native_to_bin(std::vector<char>& bin, const bytes& obj) {
    push_varuint32(bin, obj.data.size());
    bin.insert(bin.end(), obj.data.begin(), obj.data.end());
}

template <typename T>
void native_to_bin(std::vector<char>& bin, const std::vector<T>& obj) {
    push_varuint32(bin, obj.size());
    for (auto& v : obj) {
        native_to_bin(bin, v);
    }
}

template <typename T>
void native_to_bin(std::vector<char>& bin, const T& obj) {
    if constexpr (std::is_class_v<T>) {
        for_each_field((T*)nullptr, [&](auto* name, auto member_ptr) { //
            native_to_bin(bin, member_from_void(member_ptr, &obj));
        });
    } else {
        static_assert(std::is_trivially_copyable_v<T>);
        push_raw(bin, obj);
    }
}

bytes sql_to_bytes(const char* ch) {
    bytes result;
    if (!ch || ch[0] != '\\' || ch[1] != 'x')
        return result;
    try {
        boost::algorithm::unhex(ch + 2, ch + strlen(ch), std::back_inserter(result.data));
    } catch (...) {
        result.data.clear();
    }
    return result;
}

string readStr(const char* filename) {
    std::fstream file(filename, std::ios_base::in | std::ios_base::binary);
    file.seekg(0, std::ios_base::end);
    auto len = file.tellg();
    file.seekg(0, std::ios_base::beg);
    string result(len, 0);
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
    string           schema;
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

struct code_table_pk_scope {
    name     code;
    name     table;
    uint64_t primary_key = 0;
    name     scope;
};

template <typename F>
constexpr void for_each_field(code_table_pk_scope*, F f) {
    f("code", member_ptr<&code_table_pk_scope::code>{});
    f("table", member_ptr<&code_table_pk_scope::table>{});
    f("primary_key", member_ptr<&code_table_pk_scope::primary_key>{});
    f("scope", member_ptr<&code_table_pk_scope::scope>{});
};

struct query_contract_row_range_code_table_pk_scope {
    uint32_t            max_block_index = 0;
    code_table_pk_scope first;
    code_table_pk_scope last;
    uint32_t            max_results = 1;
};

template <typename F>
constexpr void for_each_field(query_contract_row_range_code_table_pk_scope*, F f) {
    f("max_block_index", member_ptr<&query_contract_row_range_code_table_pk_scope::max_block_index>{});
    f("first", member_ptr<&query_contract_row_range_code_table_pk_scope::first>{});
    f("last", member_ptr<&query_contract_row_range_code_table_pk_scope::last>{});
    f("max_results", member_ptr<&query_contract_row_range_code_table_pk_scope::max_results>{});
};

using query = std::variant<query_contract_row_range_code_table_pk_scope>;

struct contract_row {
    uint32_t block_index = 0;
    bool     present     = false;
    name     code;
    name     scope;
    name     table;
    uint64_t primary_key = 0;
    name     payer;
    bytes    value;
};

template <typename F>
constexpr void for_each_field(contract_row*, F f) {
    f("block_index", member_ptr<&contract_row::block_index>{});
    f("present", member_ptr<&contract_row::present>{});
    f("code", member_ptr<&contract_row::code>{});
    f("scope", member_ptr<&contract_row::scope>{});
    f("table", member_ptr<&contract_row::table>{});
    f("primary_key", member_ptr<&contract_row::primary_key>{});
    f("payer", member_ptr<&contract_row::payer>{});
    f("value", member_ptr<&contract_row::value>{});
}

bool query_db_impl(JSContext* cx, JS::CallArgs& args, unsigned callback_arg, query_contract_row_range_code_table_pk_scope& req) {
    try {
        pqxx::work t(foo_global->sql_connection);

        auto result = t.exec(
            "select * from chain.contract_row_range_code_table_pk_scope(" + //
            sql_str(req.max_block_index) + sep +                            //
            sql_str(req.first.code) + sep +                                 //
            sql_str(req.first.table) + sep +                                //
            sql_str(req.first.primary_key) + sep +                          //
            sql_str(req.first.scope) + sep +                                //
            sql_str(req.last.code) + sep +                                  //
            sql_str(req.last.table) + sep +                                 //
            sql_str(req.last.primary_key) + sep +                           //
            sql_str(req.last.scope) + sep +                                 //
            sql_str(req.max_results) + ")");

        std::vector<contract_row> v;
        for (const auto& r : result) {
            v.push_back(contract_row{
                .block_index = r[0].as<uint32_t>(),
                .present     = r[1].as<bool>(),
                .code        = name{r[2].c_str()},
                .table       = name{r[3].c_str()},
                .scope       = name{r[4].c_str()},
                .primary_key = r[5].as<uint64_t>(),
                .payer       = name{r[6].c_str()},
                .value       = sql_to_bytes(r[7].c_str()),
            });
        }
        t.commit();

        std::vector<char> bin;
        native_to_bin(bin, v);
        auto data = get_mem_from_callback(cx, args, callback_arg, bin.size());
        if (!js_assert(data, cx, "exec_query: failed to fetch buffer from callback"))
            return false;
        memcpy(data, bin.data(), bin.size());
        return true;
    } catch (...) {
        std::cerr << "!!!!! c\n";
        JS_ReportOutOfMemory(cx);
        return false;
    }
} // query_db_impl

// args: ArrayBuffer, row_request_begin, row_request_end, callback
bool exec_query(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "exec_query", 4))
        return false;
    query req{};
    bool  ok = false;
    {
        JS::AutoCheckCannotGC checkGC;
        auto                  bin = get_input_buffer(args, 0, 1, 2, checkGC);
        if (bin.pos) {
            try {
                ok = bin_to_native(req, bin);
            } catch (...) {
            }
        }
    }
    if (!ok)
        return js_assert(false, cx, "exec_query: invalid args");
    return std::visit([&](auto& r) { return query_db_impl(cx, args, 3, r); }, req);
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
        bpo::variables_map vm;
        bpo::store(bpo::parse_command_line(argc, argv, desc), vm);
        bpo::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << '\n';
            return 0;
        }

        foo_global         = std::make_unique<foo>();
        foo_global->schema = vm["schema"].as<string>();
        runit();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
