#include "map_macro.h"
#include <abieos.hpp>
#include <eosio/contract.hpp>
#include <eosio/input_output.hpp>
#include <eosio/parse_json.hpp>

namespace eosio {

template <typename... Ts>
struct type_list {};

__attribute__((noinline)) inline result<void> parse_json(name& result, json_parser::token_stream& stream) {
    auto r = stream.get_string();
    if (!r)
        return r.error();
    result = name(r.value());
    return outcome::success();
}

__attribute__((noinline)) inline result<void> parse_json(asset& result, json_parser::token_stream& stream) {
    auto r = stream.get_string();
    if (!r)
        return r.error();
    abieos::asset a;
    std::string   error;
    if (!string_to_asset(a, error, std::string(r.value()).c_str()))
        return json_parser::error::value_invalid;
    result = asset{a.amount, symbol{a.sym.value}};
    return outcome::success();
}

__attribute__((noinline)) inline result<void> parse_json(symbol& result, json_parser::token_stream& stream) {
    auto r = stream.get_string();
    if (!r)
        return r.error();
    uint64_t    sym;
    std::string error;
    if (!abieos::string_to_symbol(sym, error, std::string(r.value()).c_str()))
        return json_parser::error::value_invalid;
    result = symbol{sym};
    return outcome::success();
}

template <typename Arg0, typename... Args>
void action_json_to_bin(type_list<Arg0, Args...>, std::vector<char>& dest, json_parser::token_stream& stream) {
    std::decay_t<Arg0> obj{};
    check_discard(parse_json(obj, stream));
    auto bin = pack(obj);
    dest.insert(dest.end(), bin.begin(), bin.end());
    action_json_to_bin(type_list<Args...>{}, dest, stream);
}

void action_json_to_bin(type_list<>, std::vector<char>& dest, json_parser::token_stream& stream) {}

template <typename C, typename... Args>
void action_json_to_bin(void (C::*)(Args...), std::vector<char>& dest, json_parser::token_stream& stream) {
    action_json_to_bin(type_list<Args...>{}, dest, stream);
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
    void for_each_query(CLS*, F f) {                                                                                                       \
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

#define TRANSLATE_CONTRACT_ACTIONS(CLS)                                                                                                    \
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
        std::vector<char>                result;                                                                                           \
        auto                             json = get_input_data_str();                                                                      \
        eosio::json_parser::token_stream stream(json.data());                                                                              \
        eosio::check_discard(stream.get_start_array());                                                                                    \
        name action(eosio::check(stream.get_string()).value());                                                                            \
        bool found = false;                                                                                                                \
        for_each_action((CLS*)nullptr, [&](name name, auto action_fn) {                                                                    \
            if (!found && action == name) {                                                                                                \
                found = true;                                                                                                              \
                action_json_to_bin(action_fn, result, stream);                                                                             \
            }                                                                                                                              \
        });                                                                                                                                \
        eosio::check(found, "action not found");                                                                                           \
        eosio::check_discard(stream.get_end_array());                                                                                      \
        eosio::check_discard(stream.get_end());                                                                                            \
        set_output_data(result);                                                                                                           \
    }                                                                                                                                      \
    }
