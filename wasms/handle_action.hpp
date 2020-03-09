#pragma once

#include <eosio/asset.hpp>
#include <eosio/check.hpp>
#include <eosio/datastream.hpp>
#include <eosio/history-tools/state_history.hpp>
#include <eosio/name.hpp>
#include <eosio/print.hpp>
#include <vector>

using namespace eosio;
using namespace state_history;
using namespace abieos::literals;

namespace eosio {

ABIEOS_REFLECT(name) { //
    ABIEOS_MEMBER(name, value);
}

/*
void native_to_bin(const symbol& obj, std::vector<char>& bin) { abieos::native_to_bin(obj.raw(), bin); }

ABIEOS_NODISCARD inline bool bin_to_native(symbol& obj, abieos::bin_to_native_state& state, bool start) {
    uint64_t raw;
    if (!abieos::bin_to_native(raw, state, start))
        return false;
    obj = symbol(raw);
    return true;
}

ABIEOS_NODISCARD bool json_to_native(symbol& obj, abieos::json_to_native_state& state, abieos::event_type event, bool start) {
    check(false, "not implemented");
    return false;
}
*/

ABIEOS_REFLECT(asset) {
    ABIEOS_MEMBER(asset, amount);
    ABIEOS_MEMBER(asset, symbol);
}

// todo: symbol kv sort order
// todo: asset kv sort order

} // namespace eosio

namespace eosio {

namespace internal_use_do_not_use {

#define IMPORT extern "C" __attribute__((eosio_wasm_import))

IMPORT uint32_t get_bin(void* data, uint32_t size);

#undef IMPORT

} // namespace internal_use_do_not_use

inline const std::vector<char>& get_bin() {
    static std::optional<std::vector<char>> bytes;
    if (!bytes) {
        bytes.emplace();
        bytes->resize(internal_use_do_not_use::get_bin(nullptr, 0));
        internal_use_do_not_use::get_bin(bytes->data(), bytes->size());
    }
    return *bytes;
}

template <typename T>
T construct_from_stream(datastream<const char*>& ds) {
    T obj{};
    ds >> obj;
    return obj;
}

template <typename... Ts>
struct type_list {};

template <int i, typename... Ts>
struct skip;

template <int i, typename T, typename... Ts>
struct skip<i, T, Ts...> {
    using types = typename skip<i - 1, Ts...>::type;
};

template <typename T, typename... Ts>
struct skip<1, T, Ts...> {
    using types = type_list<Ts...>;
};

template <typename F, typename... DataStreamArgs, typename... FixedArgs>
void dispatch_mixed(F f, type_list<DataStreamArgs...>, abieos::input_buffer bin, FixedArgs... fixedArgs) {
    datastream<const char*> ds(bin.pos, bin.end - bin.pos);
    std::apply(f, std::tuple<FixedArgs..., DataStreamArgs...>{fixedArgs..., construct_from_stream<DataStreamArgs>(ds)...});
}

template <typename... Ts>
struct serial_dispatcher;

template <typename C, typename... Args, typename... FixedArgs>
struct serial_dispatcher<void (C::*)(Args...) const, FixedArgs...> {
    template <typename F>
    static void dispatch(F f, abieos::input_buffer bin, FixedArgs... fixedArgs) {
        dispatch_mixed(f, typename skip<sizeof...(FixedArgs), std::decay_t<Args>...>::types{}, bin, fixedArgs...);
    }
};

struct action_context {
    const transaction_trace& ttrace;
    const action_trace&      atrace;
};

struct handle_action_base {
    abieos::name contract;
    abieos::name action;

    virtual void dispatch(const action_context& context, abieos::input_buffer bin) = 0;

    static std::vector<handle_action_base*>& get_actions() {
        static std::vector<handle_action_base*> actions;
        return actions;
    }
};

template <typename F>
struct handle_action : handle_action_base {
    F f;

    handle_action(abieos::name contract, abieos::name action, F f)
        : f(f) {
        this->contract = contract;
        this->action   = action;
        get_actions().push_back(this);
    }
    handle_action(const handle_action&) = delete;
    handle_action& operator=(const handle_action&) = delete;

    void dispatch(const action_context& context, abieos::input_buffer bin) override {
        serial_dispatcher<decltype(&F::operator()), const action_context&>::dispatch(f, bin, context);
    }
};

} // namespace eosio
