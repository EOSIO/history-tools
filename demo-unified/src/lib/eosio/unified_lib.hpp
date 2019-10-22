#include "map_macro.h"
#include <abieos.hpp>
#include <eosio/contract.hpp>
#include <eosio/input_output.hpp>
#include <eosio/parse_json.hpp>

namespace eosio {

template <typename... Ts>
struct type_list {};

__attribute__((noinline)) inline void parse_json(asset& result, const char*& pos, const char* end) {
    std::string s;
    parse_json(s, pos, end);
    abieos::asset a;
    std::string   error;
    if (!string_to_asset(a, error, s.c_str()))
        check(false, error.c_str());
    result = asset{a.amount, symbol{a.sym.value}};
}

__attribute__((noinline)) inline void parse_json(symbol& result, const char*& pos, const char* end) {
    std::string s;
    parse_json(s, pos, end);
    uint64_t    sym;
    std::string error;
    if (!abieos::string_to_symbol(sym, error, s.c_str()))
        check(false, error.c_str());
    result = symbol{sym};
}

template <typename Arg0, typename... Args>
void action_json_to_bin(type_list<Arg0, Args...>, std::vector<char>& dest, const char*& pos, const char* end) {
    parse_json_expect(pos, end, ',', "expected ,");
    std::decay_t<Arg0> obj{};
    parse_json(obj, pos, end);
    auto bin = pack(obj);
    dest.insert(dest.end(), bin.begin(), bin.end());
    action_json_to_bin(type_list<Args...>{}, dest, pos, end);
}

void action_json_to_bin(type_list<>, std::vector<char>& dest, const char*& pos, const char* end) {}

template <typename C, typename... Args>
void action_json_to_bin(void (C::*)(Args...), std::vector<char>& dest, const char*& pos, const char* end) {
    action_json_to_bin(type_list<Args...>{}, dest, pos, end);
}

} // namespace eosio

#define CONTRACT_ACTIONS_INTERNAL(CLS, ACT) f(eosio::name(#ACT), &CLS::ACT);

#define CONTRACT_ACTIONS(CLS, ...)                                                                                                         \
    template <typename F>                                                                                                                  \
    void for_each_action(CLS*, F f) {                                                                                                      \
        MAP_REUSE_ARG0(CONTRACT_ACTIONS_INTERNAL, CLS, __VA_ARGS__)                                                                        \
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
        std::vector<char> result;                                                                                                          \
        auto              json = get_input_data();                                                                                         \
        const char*       pos  = json.data();                                                                                              \
        const char*       end  = pos + json.size();                                                                                        \
        parse_json_expect(pos, end, '[', "expected [");                                                                                    \
        std::string_view action_name;                                                                                                      \
        parse_json(action_name, pos, end);                                                                                                 \
        name action(action_name);                                                                                                          \
        bool found = false;                                                                                                                \
        for_each_action((CLS*)nullptr, [&](name name, auto action_fn) {                                                                    \
            if (!found && action == name) {                                                                                                \
                found = true;                                                                                                              \
                action_json_to_bin(action_fn, result, pos, end);                                                                           \
            }                                                                                                                              \
        });                                                                                                                                \
        check(found, "action not found");                                                                                                  \
        parse_json_expect(pos, end, ']', "expected ]");                                                                                    \
        parse_json_expect_end(pos, end);                                                                                                   \
        set_output_data(result);                                                                                                           \
    }                                                                                                                                      \
    }
