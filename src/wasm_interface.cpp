// copyright defined in LICENSE.txt

#include "wasm_interface.hpp"
#include <iostream>

using namespace std::literals;

namespace wasm {

JSClassOps global_ops = {
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

JSClass global_class = {
    "global",
    JSCLASS_GLOBAL_FLAGS,
    &global_ops,
    {},
};

void create_global(wasm_state& state, const JSFunctionSpec* functions) {
    JS::RealmOptions options;
    state.global.set(JS_NewGlobalObject(state.context.cx, &global_class, nullptr, JS::FireOnNewGlobalHook, options));
    if (!state.global)
        throw std::runtime_error("JS_NewGlobalObject failed");

    JSAutoRealm realm(state.context.cx, state.global);
    if (!JS::InitRealmStandardClasses(state.context.cx))
        throw std::runtime_error("JS::InitRealmStandardClasses failed");
    if (!JS_DefineFunctions(state.context.cx, state.global, functions))
        throw std::runtime_error("JS_DefineFunctions failed");
    if (!JS_DefineProperty(state.context.cx, state.global, "global", state.global, 0))
        throw std::runtime_error("JS_DefineProperty failed");
}

void execute(wasm_state& state, const char* filename, std::string_view script) {
    JSAutoRealm        realm(state.context.cx, state.global);
    JS::CompileOptions opts(state.context.cx);
    opts.setFileAndLine(filename, 1);
    JS::RootedValue rval(state.context.cx);
    bool            ok = JS::EvaluateUtf8(state.context.cx, opts, script.begin(), script.size(), &rval);
    if (!ok)
        throw std::runtime_error("JS::Evaluate failed");
}

bool convert_str(JSContext* cx, JSString* str, std::string& dest) {
    auto len = JS_GetStringEncodingLength(cx, str);
    if (len == size_t(-1))
        return false;
    dest.clear();
    dest.resize(len);
    return JS_EncodeStringToBuffer(cx, str, dest.data(), len);
}

abieos::input_buffer
get_input_buffer(JS::CallArgs& args, unsigned buffer_arg, unsigned begin_arg, unsigned end_arg, JS::AutoRequireNoGC& nogc) {
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

bool print_js_str(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    std::string  s;
    for (unsigned i = 0; i < args.length(); ++i)
        if (args[i].isString() && convert_str(cx, args[i].toString(), s))
            std::cerr << s;
    return true;
}

bool print_wasm_str(JSContext* cx, unsigned argc, JS::Value* vp) {
    auto&        state = wasm_state::from_context(cx);
    JS::CallArgs args  = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "print_wasm_str", 3))
        return false;
    if (!state.console)
        return true;
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

// args: callback
bool get_context_data(JSContext* cx, unsigned argc, JS::Value* vp) {
    auto&        state = wasm_state::from_context(cx);
    JS::CallArgs args  = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "get_context_data", 1))
        return false;
    try {
        auto data = get_mem_from_callback(cx, args, 0, state.context_data.size());
        if (!js_assert(data, cx, "get_context_data: failed to fetch buffer from callback"))
            return false;
        memcpy(data, state.context_data.data(), state.context_data.size());
        return true;
    } catch (const std::exception& e) {
        return js_assert(false, cx, ("get_context_data: "s + e.what()).c_str());
    } catch (...) {
        return js_assert(false, cx, "get_context_data error");
    }
} // get_context_data

// args: callback
bool get_input_data(JSContext* cx, unsigned argc, JS::Value* vp) {
    auto&        state = wasm_state::from_context(cx);
    JS::CallArgs args  = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "get_input_data", 1))
        return false;
    try {
        auto size = state.request.end - state.request.pos;
        if (!js_assert((uint32_t)size == size, cx, "get_input_data: request is too big"))
            return false;
        auto data = get_mem_from_callback(cx, args, 0, size);
        if (!js_assert(data, cx, "get_input_data: failed to fetch buffer from callback"))
            return false;
        memcpy(data, state.request.pos, size);
        return true;
    } catch (const std::exception& e) {
        return js_assert(false, cx, ("get_input_data: "s + e.what()).c_str());
    } catch (...) {
        return js_assert(false, cx, "get_input_data error");
    }
} // get_input_data

// args: ArrayBuffer, row_request_begin, row_request_end
bool set_output_data(JSContext* cx, unsigned argc, JS::Value* vp) {
    auto&        state = wasm_state::from_context(cx);
    JS::CallArgs args  = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "set_output_data", 3))
        return false;
    bool ok = true;
    {
        JS::AutoCheckCannotGC checkGC;
        auto                  b = get_input_buffer(args, 0, 1, 2, checkGC);
        if (b.pos) {
            try {
                state.reply.assign(b.pos, b.end);
            } catch (...) {
                ok = false;
            }
        }
    }
    return js_assert(ok, cx, "set_output_data: invalid args");
}

} // namespace wasm
