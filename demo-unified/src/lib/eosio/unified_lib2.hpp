#pragma once

#ifdef __eosio_cdt__
#include <cwchar>
namespace std {
using ::wcslen;
}
#endif

#include <eosio/datastream.hpp>
#include <eosio/stream.hpp>

#define asset og_asset
#define extended_asset og_extended_asset
#define extended_symbol og_extended_symbol
#define name og_name
#define public_key og_public_key
#define signature og_signature
#define symbol og_symbol
#define symbol_code og_symbol_code
#define webauthn_public_key og_webauthn_public_key
#define webauthn_signature og_webauthn_signature

#include <../core/eosio/asset.hpp>
#include <../core/eosio/crypto.hpp>
#include <../core/eosio/name.hpp>
#include <../core/eosio/symbol.hpp>

#undef asset
#undef extended_asset
#undef extended_symbol
#undef name
#undef public_key
#undef signature
#undef symbol
#undef symbol_code
#undef webauthn_public_key
#undef webauthn_signature

#include <../../abieos/include/eosio/name.hpp>
#include <../../abieos/include/eosio/symbol.hpp>

#include <../../abieos/include/eosio/asset.hpp>
#include <../../abieos/include/eosio/crypto.hpp>

#include <eosio/print.hpp>

namespace eosio {

template <typename DataStream>
DataStream& operator<<(DataStream& ds, const name& v) {
    ds << v.value;
    return ds;
}

template <typename DataStream>
DataStream& operator>>(DataStream& ds, name& v) {
    ds >> v.value;
    return ds;
}

inline void print(const name& n) { print((std::string)n); }

} // namespace eosio

#include <eosio/action.hpp>
#include <eosio/asset.hpp>
#include <eosio/contract.hpp>
#include <eosio/from_bin.hpp>
#include <eosio/from_json.hpp>
#include <eosio/input_output.hpp>
#include <eosio/to_bin.hpp>
#include <eosio/to_json.hpp>

namespace eosio {

namespace internal_use_do_not_use {
extern "C" void set_action_return_value(char* return_value, size_t size);
}

namespace globals {
extern name self;
}

inline name get_self() { return globals::self; }

template <typename... Ts>
struct type_list {};

// template <typename S>
// result<void> from_json(asset& result, S& stream) {
//     auto r = stream.get_string();
//     if (!r)
//         return r.error();
//     eosio::asset a;
//     if (!string_to_asset(a, r.value().data(), r.value().data() + r.value().size()))
//         return from_json_error::value_invalid;
//     result = asset{a.amount, symbol{a.symbol}};
//     return outcome::success();
// }

// template <typename S>
// result<void> from_json(symbol& result, S& stream) {
//     auto r = stream.get_string();
//     if (!r)
//         return r.error();
//     uint64_t sym;
//     if (!eosio::string_to_symbol(sym, r.value().data(), r.value().data() + r.value().size()))
//         return from_json_error::value_invalid;
//     result = symbol{sym};
//     return outcome::success();
// }

// template <typename S>
// result<void> to_json(const asset& a, S& stream) {
//     return to_json(a.to_string(), stream);
// }

// todo: named-args format
template <typename Arg0, typename... Args, typename S>
void args_json_to_bin(type_list<Arg0, Args...>, std::vector<char>& dest, S& stream) {
    std::decay_t<Arg0> obj{};
    check_discard(from_json(obj, stream));
    std::vector<char>    bin;
    eosio::vector_stream bin_stream{bin};
    eosio::check_discard(to_bin(obj, bin_stream));
    dest.insert(dest.end(), bin.begin(), bin.end());
    args_json_to_bin(type_list<Args...>{}, dest, stream);
}

template <typename S>
void args_json_to_bin(type_list<>, std::vector<char>& dest, S& stream) {}

template <typename R, typename... Args, typename S>
void args_json_to_bin(R (*)(Args...), std::vector<char>& dest, S& stream) {
    args_json_to_bin(type_list<Args...>{}, dest, stream);
}

template <typename R, typename... Args>
inline std::string ret_bin_to_json(R (*)(Args...), datastream<const char*>& ds) {
    R                   ret{};
    eosio::input_stream is{ds.pos(), ds.pos() + ds.remaining()};
    eosio::check_discard(from_bin(ret, is));
    return check(convert_to_json(ret)).value();
}

template <typename Ret, typename... Args>
void execute_action2(name self, name code, Ret (*f)(Args...)) {
    size_t size   = action_data_size();
    char*  buffer = nullptr;
    if (size) {
        buffer = (char*)malloc(size);
        read_action_data(buffer, size);
    }
    datastream<const char*>           ds(buffer, size);
    std::tuple<std::decay_t<Args>...> args;
    ds >> args;
    if constexpr (std::is_same_v<Ret, void>) {
        std::apply(f, args);
    } else {
        auto result = pack(std::apply(f, args));
        internal_use_do_not_use::set_action_return_value(result.data(), result.size());
    }
}

template <typename R, typename... Args>
void execute_query(name self, name name, R (*f)(Args...)) {
    auto                              input_data = get_input_data();
    datastream<const char*>           ds(input_data.data(), input_data.size());
    std::tuple<std::decay_t<Args>...> args;
    ds >> args;
    std::apply(
        [self, name, f, &ds](auto&... a) {
            auto              result = f(a...);
            std::vector<char> result_data(pack_size(name) + pack_size(result));
            datastream<char*> result_ds(result_data.data(), result_data.size());
            result_ds << name << result;
            set_output_data(result_data);
        },
        args);
}

} // namespace eosio

#define CONTRACT_ACTIONS_INTERNAL(DUMMY, ACT) f(eosio::name(#ACT), action_##ACT);

#define CONTRACT_ACTIONS(...)                                                                                                              \
    template <typename F>                                                                                                                  \
    void for_each_action(F f) {                                                                                                            \
        EOSIO_MAP_REUSE_ARG0(CONTRACT_ACTIONS_INTERNAL, dummy, __VA_ARGS__)                                                                \
    }

#define CONTRACT_QUERIES_INTERNAL(DUMMY, QUERY) f(eosio::name(#QUERY), query_##QUERY);

#define CONTRACT_QUERIES(...)                                                                                                              \
    template <typename F>                                                                                                                  \
    inline void for_each_query(F f) {                                                                                                      \
        EOSIO_MAP_REUSE_ARG0(CONTRACT_QUERIES_INTERNAL, dummy, __VA_ARGS__)                                                                \
    }

#define DISPATCH_CONTRACT_ACTIONS()                                                                                                        \
    namespace eosio::globals {                                                                                                             \
    name self;                                                                                                                             \
    }                                                                                                                                      \
                                                                                                                                           \
    extern "C" {                                                                                                                           \
    [[eosio::wasm_entry]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {                                                  \
        globals::self.value = receiver;                                                                                                    \
        if (code == receiver) {                                                                                                            \
            bool found = false;                                                                                                            \
            for_each_action([=, &found](eosio::name name, auto action_fn) {                                                                \
                if (!found && action == name.value) {                                                                                      \
                    found = true;                                                                                                          \
                    eosio::execute_action2(eosio::name(receiver), eosio::name(code), action_fn);                                           \
                }                                                                                                                          \
            });                                                                                                                            \
            eosio::check(found, "unknown action");                                                                                         \
        }                                                                                                                                  \
    }                                                                                                                                      \
    }

#define DISPATCH_CONTRACT_QUERIES()                                                                                                        \
    namespace eosio::globals {                                                                                                             \
    name self;                                                                                                                             \
    }                                                                                                                                      \
                                                                                                                                           \
    extern "C" {                                                                                                                           \
    [[eosio::wasm_entry]] void initialize() {}                                                                                             \
                                                                                                                                           \
    [[eosio::wasm_entry]] void run_query(eosio::name self, eosio::name query) {                                                            \
        globals::self = self;                                                                                                              \
        bool found    = false;                                                                                                             \
        for_each_query([=, &found](eosio::name name, auto query_fn) {                                                                      \
            if (!found && query == name) {                                                                                                 \
                found = true;                                                                                                              \
                eosio::execute_query(self, name, query_fn);                                                                                \
            }                                                                                                                              \
        });                                                                                                                                \
        eosio::check(found, "unknown query");                                                                                              \
    }                                                                                                                                      \
    }

#define TRANSLATE_CONTRACT_ACTIONS_AND_QUERIES()                                                                                           \
    extern "C" {                                                                                                                           \
    [[eosio::wasm_entry]] void initialize() {}                                                                                             \
                                                                                                                                           \
    [[eosio::wasm_entry]] const char* get_actions() {                                                                                      \
        static std::string result;                                                                                                         \
        static bool        initialized = false;                                                                                            \
        if (!initialized) {                                                                                                                \
            result          = "[";                                                                                                         \
            bool need_comma = false;                                                                                                       \
            for_each_action([&](eosio::name name, auto action_fn) {                                                                        \
                if (need_comma)                                                                                                            \
                    result += ",";                                                                                                         \
                need_comma = true;                                                                                                         \
                result += "{\"name\":\"" + (std::string)name + "\"}";                                                                      \
            });                                                                                                                            \
            result += "]";                                                                                                                 \
            initialized = true;                                                                                                            \
        }                                                                                                                                  \
        return result.c_str();                                                                                                             \
    }                                                                                                                                      \
                                                                                                                                           \
    [[eosio::wasm_entry]] void action_to_bin() {                                                                                           \
        using namespace eosio;                                                                                                             \
        std::vector<char>        result;                                                                                                   \
        auto                     json = get_input_data_str();                                                                              \
        eosio::json_token_stream stream(json.data());                                                                                      \
        eosio::check_discard(stream.get_start_array());                                                                                    \
        name action(eosio::check(stream.get_string()).value());                                                                            \
        bool found = false;                                                                                                                \
        for_each_action([&](eosio::name name, auto action_fn) {                                                                            \
            if (!found && action == name) {                                                                                                \
                found = true;                                                                                                              \
                args_json_to_bin(action_fn, result, stream);                                                                               \
            }                                                                                                                              \
        });                                                                                                                                \
        eosio::check(found, "action not found");                                                                                           \
        eosio::check_discard(stream.get_end_array());                                                                                      \
        eosio::check_discard(stream.get_end());                                                                                            \
        set_output_data(result);                                                                                                           \
    }                                                                                                                                      \
                                                                                                                                           \
    [[eosio::wasm_entry]] const char* get_queries() {                                                                                      \
        static std::string result;                                                                                                         \
        static bool        initialized = false;                                                                                            \
        if (!initialized) {                                                                                                                \
            result          = "[";                                                                                                         \
            bool need_comma = false;                                                                                                       \
            for_each_query([&](eosio::name name, auto query_fn) {                                                                          \
                if (need_comma)                                                                                                            \
                    result += ",";                                                                                                         \
                need_comma = true;                                                                                                         \
                result += "{\"name\":\"" + (std::string)name + "\"}";                                                                      \
            });                                                                                                                            \
            result += "]";                                                                                                                 \
            initialized = true;                                                                                                            \
        }                                                                                                                                  \
        return result.c_str();                                                                                                             \
    }                                                                                                                                      \
                                                                                                                                           \
    [[eosio::wasm_entry]] void query_to_bin() {                                                                                            \
        using namespace eosio;                                                                                                             \
        std::vector<char>        result;                                                                                                   \
        auto                     json = get_input_data_str();                                                                              \
        eosio::json_token_stream stream(json.data());                                                                                      \
        eosio::check_discard(stream.get_start_array());                                                                                    \
        name query(eosio::check(stream.get_string()).value());                                                                             \
        bool found = false;                                                                                                                \
        for_each_query([&](name name, auto query_fn) {                                                                                     \
            if (!found && query == name) {                                                                                                 \
                found = true;                                                                                                              \
                args_json_to_bin(query_fn, result, stream);                                                                                \
            }                                                                                                                              \
        });                                                                                                                                \
        eosio::check(found, "query not found");                                                                                            \
        eosio::check_discard(stream.get_end_array());                                                                                      \
        eosio::check_discard(stream.get_end());                                                                                            \
        set_output_data(result);                                                                                                           \
    }                                                                                                                                      \
                                                                                                                                           \
    [[eosio::wasm_entry]] void query_result_to_json() {                                                                                    \
        using namespace eosio;                                                                                                             \
        auto                    bin = get_input_data();                                                                                    \
        datastream<const char*> ds{bin.data(), bin.size()};                                                                                \
        eosio::name             query;                                                                                                     \
        ds >> query;                                                                                                                       \
        std::string json;                                                                                                                  \
        bool        found = false;                                                                                                         \
        for_each_query([&](name name, auto query_fn) {                                                                                     \
            if (!found && query == name) {                                                                                                 \
                found = true;                                                                                                              \
                json  = ret_bin_to_json(query_fn, ds);                                                                                     \
            }                                                                                                                              \
        });                                                                                                                                \
        eosio::check(found, "query not found");                                                                                            \
        set_output_data(json);                                                                                                             \
    }                                                                                                                                      \
    }

#if defined(CLIENT_WASM)
#define DISPATCH() TRANSLATE_CONTRACT_ACTIONS_AND_QUERIES()
#elif defined(QUERY_WASM)
#define DISPATCH() DISPATCH_CONTRACT_QUERIES()
#else
#define DISPATCH() DISPATCH_CONTRACT_ACTIONS()
#endif
