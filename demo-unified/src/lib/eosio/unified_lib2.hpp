#include "map_macro.h"
#include <abieos.hpp>
#include <eosio/contract.hpp>
#include <eosio/from_json.hpp>
#include <eosio/input_output.hpp>
#include <eosio/to_json.hpp>

namespace eosio {

namespace globals {
extern name self;
}

inline name get_self() { return globals::self; }

template <typename... Ts>
struct type_list {};

template <typename S>
result<void> from_json(name& result, S& stream) {
    auto r = stream.get_string();
    if (!r)
        return r.error();
    result = name(r.value());
    return outcome::success();
}

template <typename S>
result<void> from_json(asset& result, S& stream) {
    auto r = stream.get_string();
    if (!r)
        return r.error();
    abieos::asset a;
    std::string   error;
    if (!string_to_asset(a, error, r.value().data(), r.value().data() + r.value().size()))
        return from_json_error::value_invalid;
    result = asset{a.amount, symbol{a.sym.value}};
    return outcome::success();
}

template <typename S>
result<void> from_json(symbol& result, S& stream) {
    auto r = stream.get_string();
    if (!r)
        return r.error();
    uint64_t sym;
    if (!eosio::string_to_symbol(sym, r.value().data(), r.value().data() + r.value().size()))
        return from_json_error::value_invalid;
    result = symbol{sym};
    return outcome::success();
}

template <typename S>
result<void> to_json(const asset& a, S& stream) {
    return to_json(a.to_string(), stream);
}

// todo: named-args format
template <typename Arg0, typename... Args, typename S>
void args_json_to_bin(type_list<Arg0, Args...>, std::vector<char>& dest, S& stream) {
    std::decay_t<Arg0> obj{};
    check_discard(from_json(obj, stream));
    auto bin = pack(obj);
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
    R ret{};
    ds >> ret;
    return check(to_json(ret)).value();
}

template <typename... Args>
void execute_action2(name self, name code, void (*f)(Args...)) {
    size_t size   = action_data_size();
    char*  buffer = nullptr;
    if (size) {
        buffer = (char*)malloc(size);
        read_action_data(buffer, size);
    }
    datastream<const char*>           ds(buffer, size);
    std::tuple<std::decay_t<Args>...> args;
    ds >> args;
    std::apply(f, args);
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

#define CONTRACT_ACTIONS_INTERNAL(ACT) f(eosio::name(#ACT), action_##ACT);

#define CONTRACT_ACTIONS(...)                                                                                                              \
    template <typename F>                                                                                                                  \
    void for_each_action(F f) {                                                                                                            \
        MAP(CONTRACT_ACTIONS_INTERNAL, __VA_ARGS__)                                                                                        \
    }

#define CONTRACT_QUERIES_INTERNAL(QUERY) f(eosio::name(#QUERY), query_##QUERY);

#define CONTRACT_QUERIES(...)                                                                                                              \
    template <typename F>                                                                                                                  \
    inline void for_each_query(F f) {                                                                                                      \
        MAP(CONTRACT_QUERIES_INTERNAL, __VA_ARGS__)                                                                                        \
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
