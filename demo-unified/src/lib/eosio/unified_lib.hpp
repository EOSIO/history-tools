#include "map_macro.h"
#include <abieos.hpp>
#include <eosio/contract.hpp>

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
                result += "\"" + name.to_string() + "\"";                                                                                  \
            });                                                                                                                            \
            result += "]";                                                                                                                 \
            initialized = true;                                                                                                            \
        }                                                                                                                                  \
        return result.c_str();                                                                                                             \
    }                                                                                                                                      \
                                                                                                                                           \
    [[eosio::wasm_entry]] void action_to_bin() {}                                                                                          \
    }
