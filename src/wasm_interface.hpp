// copyright defined in LICENSE.txt

#pragma once

#include "abieos.hpp"

#include "jsapi.h"

#include "js/CompilationAndEvaluation.h"
#include "js/Initialization.h"
#include "jsfriendapi.h"

namespace wasm {

struct context_wrapper {
    JSContext* cx;

    context_wrapper() {
        cx = JS_NewContext(8L * 1024 * 1024);
        if (!cx)
            throw std::runtime_error("JS_NewContext failed");
        if (!JS::InitSelfHostedCode(cx)) {
            JS_DestroyContext(cx);
            throw std::runtime_error("JS::InitSelfHostedCode failed");
        }
    }

    ~context_wrapper() { JS_DestroyContext(cx); }
};

struct wasm_state {
    context_wrapper      context;
    JS::RootedObject     global;
    bool                 console         = {};
    std::vector<char>    database_status = {};
    abieos::input_buffer request         = {}; // todo: rename
    std::vector<char>    reply           = {}; // todo: rename

    wasm_state()
        : global(context.cx) {
        JS_SetContextPrivate(context.cx, this);
    }

    static wasm_state& from_context(JSContext* cx) { return *reinterpret_cast<wasm_state*>(JS_GetContextPrivate(cx)); }
};

inline bool js_assert(bool cond, JSContext* cx, const char* s) {
    if (!cond)
        JS_ReportErrorUTF8(cx, "%s", s);
    return cond;
}

void create_global(wasm_state& state, const JSFunctionSpec* functions);
void execute(wasm_state& state, const char* filename, std::string_view script);

bool convert_str(JSContext* cx, JSString* str, std::string& dest);
abieos::input_buffer
      get_input_buffer(JS::CallArgs& args, unsigned buffer_arg, unsigned begin_arg, unsigned end_arg, JS::AutoRequireNoGC& nogc);
char* get_mem_from_callback(JSContext* cx, JS::CallArgs& args, unsigned callback_arg, int32_t size);

bool print_js_str(JSContext* cx, unsigned argc, JS::Value* vp);
bool print_wasm_str(JSContext* cx, unsigned argc, JS::Value* vp);
bool get_database_status(JSContext* cx, unsigned argc, JS::Value* vp);
bool get_input_data(JSContext* cx, unsigned argc, JS::Value* vp);
bool set_output_data(JSContext* cx, unsigned argc, JS::Value* vp);

} // namespace wasm
