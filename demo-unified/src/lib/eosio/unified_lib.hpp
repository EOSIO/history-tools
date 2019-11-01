#include "map_macro.h"
#include <abieos.hpp>
#include <eosio/contract.hpp>
#include <eosio/input_output.hpp>
#include <eosio/parse_json.hpp>
#include <eosio/to_json.hpp>

namespace eosio {

template <typename... Ts>
struct type_list {};

__attribute__((noinline)) inline result<void> parse_json(name& result, json_token_stream& stream) {
    auto r = stream.get_string();
    if (!r)
        return r.error();
    result = name(r.value());
    return outcome::success();
}

__attribute__((noinline)) inline result<void> parse_json(asset& result, json_token_stream& stream) {
    auto r = stream.get_string();
    if (!r)
        return r.error();
    abieos::asset a;
    std::string   error;
    if (!string_to_asset(a, error, std::string(r.value()).c_str()))
        return parse_json_error::value_invalid;
    result = asset{a.amount, symbol{a.sym.value}};
    return outcome::success();
}

__attribute__((noinline)) inline result<void> parse_json(symbol& result, json_token_stream& stream) {
    auto r = stream.get_string();
    if (!r)
        return r.error();
    uint64_t    sym;
    std::string error;
    if (!abieos::string_to_symbol(sym, error, std::string(r.value()).c_str()))
        return parse_json_error::value_invalid;
    result = symbol{sym};
    return outcome::success();
}

template <typename S>
result<void> to_json(const asset& a, S& stream) {
    return to_json(a.to_string(), stream);
}

// todo: named-args format
template <typename Arg0, typename... Args>
void args_json_to_bin(type_list<Arg0, Args...>, std::vector<char>& dest, json_token_stream& stream) {
    std::decay_t<Arg0> obj{};
    check_discard(parse_json(obj, stream));
    auto bin = pack(obj);
    dest.insert(dest.end(), bin.begin(), bin.end());
    args_json_to_bin(type_list<Args...>{}, dest, stream);
}

__attribute__((noinline)) inline void args_json_to_bin(type_list<>, std::vector<char>& dest, json_token_stream& stream) {}

template <typename C, typename R, typename... Args>
void args_json_to_bin(R (C::*)(Args...), std::vector<char>& dest, json_token_stream& stream) {
    args_json_to_bin(type_list<Args...>{}, dest, stream);
}

template <typename C, typename R, typename... Args>
inline std::string ret_bin_to_json(R (C::*)(Args...), datastream<const char*>& ds) {
    R ret{};
    ds >> ret;
    return check(to_json(ret)).value();
}

template <typename C, typename R, typename... Args>
void execute_query(name self, name name, R (C::*f)(Args...)) {
    auto                              input_data = get_input_data();
    datastream<const char*>           ds(input_data.data(), input_data.size());
    std::tuple<std::decay_t<Args>...> args;
    ds >> args;
    std::apply(
        [self, name, f, &ds](auto&... a) {
            C                 contract{self, ""_n, ds};
            auto              result = (contract.*f)(a...);
            std::vector<char> result_data(pack_size(name) + pack_size(result));
            datastream<char*> result_ds(result_data.data(), result_data.size());
            result_ds << name << result;
            set_output_data(result_data);
        },
        args);
}

} // namespace eosio

#define CONTRACT_ACTIONS_INTERNAL(CLS, ACT) f(eosio::name(#ACT), &CLS::ACT);

#define CONTRACT_ACTIONS(CLS, ...)                                                                                                         \
    template <typename F>                                                                                                                  \
    void for_each_action(CLS*, F f) {                                                                                                      \
        MAP_REUSE_ARG0(CONTRACT_ACTIONS_INTERNAL, CLS, __VA_ARGS__)                                                                        \
    }

#define CONTRACT_QUERIES_INTERNAL(CLS, QUERY) f(eosio::name(#QUERY), &CLS::QUERY);

#define CONTRACT_QUERIES(CLS, ...)                                                                                                         \
    template <typename F>                                                                                                                  \
    inline void for_each_query(CLS*, F f) {                                                                                                \
        MAP_REUSE_ARG0(CONTRACT_QUERIES_INTERNAL, CLS, __VA_ARGS__)                                                                        \
    }

#define DISPATCH_CONTRACT_ACTIONS(CLS)                                                                                                     \
    extern "C" {                                                                                                                           \
    [[eosio::wasm_entry]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {                                                  \
        if (code == receiver) {                                                                                                            \
            bool found = false;                                                                                                            \
            for_each_action((CLS*)nullptr, [=, &found](eosio::name name, auto action_fn) {                                                 \
                if (!found && action == name.value) {                                                                                      \
                    found = true;                                                                                                          \
                    eosio::execute_action(eosio::name(receiver), eosio::name(code), action_fn);                                            \
                }                                                                                                                          \
            });                                                                                                                            \
            eosio::check(found, "unknown action");                                                                                         \
        }                                                                                                                                  \
    }                                                                                                                                      \
    }

#define DISPATCH_CONTRACT_QUERIES(CLS)                                                                                                     \
    extern "C" {                                                                                                                           \
    [[eosio::wasm_entry]] void initialize() {}                                                                                             \
                                                                                                                                           \
    [[eosio::wasm_entry]] void run_query(eosio::name self, eosio::name query) {                                                            \
        bool found = false;                                                                                                                \
        for_each_query((CLS*)nullptr, [=, &found](eosio::name name, auto query_fn) {                                                       \
            if (!found && query == name) {                                                                                                 \
                found = true;                                                                                                              \
                eosio::execute_query(self, name, query_fn);                                                                                \
            }                                                                                                                              \
        });                                                                                                                                \
        eosio::check(found, "unknown query");                                                                                              \
    }                                                                                                                                      \
    }

#define TRANSLATE_CONTRACT_ACTIONS_AND_QUERIES(CLS)                                                                                        \
    extern "C" {                                                                                                                           \
    [[eosio::wasm_entry]] void initialize() {}                                                                                             \
                                                                                                                                           \
    [[eosio::wasm_entry]] const char* get_actions() {                                                                                      \
        static std::string result;                                                                                                         \
        static bool        initialized = false;                                                                                            \
        if (!initialized) {                                                                                                                \
            result          = "[";                                                                                                         \
            bool need_comma = false;                                                                                                       \
            for_each_action((CLS*)nullptr, [&](eosio::name name, auto action_fn) {                                                         \
                if (need_comma)                                                                                                            \
                    result += ",";                                                                                                         \
                need_comma = true;                                                                                                         \
                result += "{\"name\":\"" + name.to_string() + "\"}";                                                                       \
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
        for_each_action((CLS*)nullptr, [&](name name, auto action_fn) {                                                                    \
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
            for_each_query((CLS*)nullptr, [&](eosio::name name, auto query_fn) {                                                           \
                if (need_comma)                                                                                                            \
                    result += ",";                                                                                                         \
                need_comma = true;                                                                                                         \
                result += "{\"name\":\"" + name.to_string() + "\"}";                                                                       \
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
        for_each_query((CLS*)nullptr, [&](name name, auto query_fn) {                                                                      \
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
        for_each_query((CLS*)nullptr, [&](name name, auto query_fn) {                                                                      \
            if (!found && query == name) {                                                                                                 \
                found = true;                                                                                                              \
                json  = ret_bin_to_json(query_fn, ds);                                                                                     \
            }                                                                                                                              \
        });                                                                                                                                \
        eosio::check(found, "query not found");                                                                                            \
        set_output_data(json);                                                                                                             \
    }                                                                                                                                      \
    }
