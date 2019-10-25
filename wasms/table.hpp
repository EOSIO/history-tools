#pragma once

#include "../src/state_history.hpp"
#include <eosio/asset.hpp>
#include <eosio/check.hpp>
#include <eosio/datastream.hpp>
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

ABIEOS_REFLECT(asset) {
    ABIEOS_MEMBER(asset, amount);
    ABIEOS_MEMBER(asset, symbol);
}

// todo: symbol kv sort order
// todo: asset kv sort order

} // namespace eosio

namespace abieos {

template <typename T>
inline constexpr bool serial_reversible = std::is_signed_v<T> || std::is_unsigned_v<T>;

template <>
inline constexpr bool serial_reversible<abieos::name> = true;

template <>
inline constexpr bool serial_reversible<eosio::name> = true;

template <>
inline constexpr bool serial_reversible<abieos::uint128> = true;

template <>
inline constexpr bool serial_reversible<abieos::checksum256> = true;

template <typename F>
void reverse_bin(std::vector<char>& bin, F f) {
    auto s = bin.size();
    f();
    std::reverse(bin.begin() + s, bin.end());
}

template <typename T, typename F>
auto fixup_key(std::vector<char>& bin, F f) -> std::enable_if_t<serial_reversible<T>, void> {
    reverse_bin(bin, f);
}

template <typename T>
void native_to_key(const T& obj, std::vector<char>& bin);

void native_to_key(const std::string& obj, std::vector<char>& bin) {
    for (auto ch : obj) {
        if (ch) {
            bin.push_back(ch);
        } else {
            bin.push_back(0);
            bin.push_back(0);
        }
    }
    bin.push_back(0);
    bin.push_back(1);
}

template <int i, typename... Ts>
void native_to_key_tuple(const std::tuple<Ts...>& obj, std::vector<char>& bin) {
    if constexpr (i < sizeof...(Ts)) {
        native_to_key(std::get<i>(obj), bin);
        native_to_key_tuple<i + 1>(obj, bin);
    }
}

template <typename... Ts>
void native_to_key(const std::tuple<Ts...>& obj, std::vector<char>& bin) {
    native_to_key_tuple<0>(obj, bin);
}

template <typename T>
void native_to_key(const T& obj, std::vector<char>& bin) {
    if constexpr (serial_reversible<std::decay_t<T>>) {
        fixup_key<T>(bin, [&] { abieos::native_to_bin(obj, bin); });
    } else {
        for_each_field((T*)nullptr, [&](auto* name, auto member_ptr) { //
            native_to_key(member_from_void(member_ptr, &obj), bin);
        });
    }
}

template <typename T>
std::vector<char> native_to_key(const T& obj) {
    std::vector<char> bin;
    native_to_key(obj, bin);
    return bin;
}

} // namespace abieos

namespace eosio {
namespace internal_use_do_not_use {

#define IMPORT extern "C" __attribute__((eosio_wasm_import))

IMPORT void get_bin(void* cb_alloc_data, void* (*cb_alloc)(void* cb_alloc_data, size_t size));

IMPORT void kv_set(const char* k_begin, const char* k_end, const char* v_begin, const char* v_end);
IMPORT void kv_erase(const char* k_begin, const char* k_end);
IMPORT uint32_t kv_it_create(const char* prefix, uint32_t size);
IMPORT void     kv_it_destroy(uint32_t index);
IMPORT bool     kv_it_is_end(uint32_t index);
IMPORT int      kv_it_compare(uint32_t a, uint32_t b);
IMPORT bool     kv_it_move_to_begin(uint32_t index);
IMPORT void     kv_it_move_to_end(uint32_t index);
IMPORT void     kv_it_lower_bound(uint32_t index, const char* key, uint32_t size);
IMPORT bool     kv_it_key_matches(uint32_t index, const char* key, uint32_t size);
IMPORT bool     kv_it_incr(uint32_t index);
IMPORT int32_t kv_it_key(uint32_t index, uint32_t offset, char* dest, uint32_t size);
IMPORT int32_t kv_it_value(uint32_t index, uint32_t offset, char* dest, uint32_t size);

template <typename Alloc_fn>
inline void get_bin(Alloc_fn alloc_fn) {
    return get_bin(&alloc_fn, [](void* cb_alloc_data, size_t size) -> void* { //
        return (*reinterpret_cast<Alloc_fn*>(cb_alloc_data))(size);
    });
}

void kv_set(const std::vector<char>& k, const std::vector<char>& v) {
    kv_set(k.data(), k.data() + k.size(), v.data(), v.data() + v.size());
}

} // namespace internal_use_do_not_use

inline const std::vector<char>& get_bin() {
    static std::optional<std::vector<char>> bytes;
    if (!bytes) {
        internal_use_do_not_use::get_bin([&](size_t size) {
            bytes.emplace();
            bytes->resize(size);
            return bytes->data();
        });
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

/////////////////////////////////////////

namespace eosio {

template <typename T>
class table;

template <typename T>
class index;

template <typename T>
class table_iterator;

template <typename T>
class table_proxy;

template <typename T>
class table {
  public:
    using index    = eosio::index<T>;
    using iterator = table_iterator<T>;
    using proxy    = table_proxy<T>;
    friend index;
    friend iterator;
    friend proxy;

    table()             = default;
    table(const table&) = delete;
    table(table&&)      = delete;

    template <typename... Indexes>
    void init(abieos::name table_context, abieos::name table_name, index& primary_index, Indexes&... secondary_indexes);

    void     insert(const T& obj);
    iterator begin();
    iterator end();

  private:
    bool                initialized{};
    std::vector<char>   prefix{};
    index*              primary_index{};
    std::vector<index*> secondary_indexes{};
};

template <typename T>
class index {
  public:
    using table    = eosio::table<T>;
    using iterator = table_iterator<T>;
    using proxy    = table_proxy<T>;
    friend table;
    friend iterator;
    friend proxy;

    index(abieos::name index_name, std::vector<char> (*get_key)(const T&))
        : index_name{index_name}
        , get_key{get_key} {}

    ~index() {
        if (temp_it)
            internal_use_do_not_use::kv_it_destroy(temp_it);
    }

    iterator begin();
    iterator end();

  private:
    abieos::name index_name                = {};
    std::vector<char> (*get_key)(const T&) = {};
    table*            t                    = {};
    bool              is_primary           = false;
    std::vector<char> prefix               = {};
    uint32_t          temp_it              = 0;

    void initialize(table* t, bool is_primary);

    uint32_t get_temp_it() {
        if (!temp_it)
            temp_it = internal_use_do_not_use::kv_it_create(prefix.data(), prefix.size());
        return temp_it;
    }
};

template <typename T>
class table_proxy {
  public:
    // If object is not already in cache then read and return it. Returns existing cached object if present.
    // Caution 1: this may return stale value if something else changed the object.
    // Caution 2: object gets destroyed when iterator is destroyed or moved or if read_fresh() is called after this.
    const T& get();

  private:
    using table    = eosio::table<T>;
    using index    = eosio::index<T>;
    using iterator = table_iterator<T>;
    friend table;
    friend index;
    friend iterator;

    iterator& it;

    table_proxy(iterator& it)
        : it{it} {}
};

template <typename T>
class table_iterator {
  public:
    using table = eosio::table<T>;
    using index = eosio::index<T>;
    using proxy = table_proxy<T>;
    friend table;
    friend index;
    friend proxy;

    table_iterator(const table_iterator&) = delete;
    table_iterator(table_iterator&&)      = default;

    ~table_iterator() {
        if (it)
            internal_use_do_not_use::kv_it_destroy(it);
    }

    table_iterator& operator=(const table_iterator&) = delete;
    table_iterator& operator=(table_iterator&&) = default;

    friend int compare(const table_iterator& a, const table_iterator& b) {
        bool a_end = !a.it || internal_use_do_not_use::kv_it_is_end(a.it);
        bool b_end = !b.it || internal_use_do_not_use::kv_it_is_end(b.it);
        if (a_end && b_end)
            return 0;
        else if (a_end && !b_end)
            return 1;
        else if (!a_end && b_end)
            return -1;
        else
            return internal_use_do_not_use::kv_it_compare(a.it, b.it);
    }

    friend bool operator==(const table_iterator& a, const table_iterator& b) { return compare(a, b) == 0; }
    friend bool operator!=(const table_iterator& a, const table_iterator& b) { return compare(a, b) != 0; }
    friend bool operator<(const table_iterator& a, const table_iterator& b) { return compare(a, b) < 0; }
    friend bool operator<=(const table_iterator& a, const table_iterator& b) { return compare(a, b) <= 0; }
    friend bool operator>(const table_iterator& a, const table_iterator& b) { return compare(a, b) > 0; }
    friend bool operator>=(const table_iterator& a, const table_iterator& b) { return compare(a, b) >= 0; }

    table_iterator& operator++() {
        if (it)
            internal_use_do_not_use::kv_it_incr(it);
        obj.reset();
        return *this;
    }

    std::vector<char> get_raw_key() {
        auto size = internal_use_do_not_use::kv_it_key(it, 0, nullptr, 0);
        check(size >= 0, "iterator read failure");
        std::vector<char> result(size);
        check(internal_use_do_not_use::kv_it_key(it, 0, result.data(), size) == size, "iterator read failure");
        return result;
    }

    std::vector<char> get_raw_value() {
        auto size = internal_use_do_not_use::kv_it_value(it, 0, nullptr, 0);
        check(size >= 0, "iterator read failure");
        std::vector<char> result(size);
        check(internal_use_do_not_use::kv_it_value(it, 0, result.data(), size) == size, "iterator read failure");
        return result;
    }

    // Reads object, stores it in cache, and returns it. Use this if something else may have changed object.
    // Caution: object gets destroyed when iterator is destroyed or moved or if read_fresh() is called again.
    const T& read_fresh() {
        auto size = internal_use_do_not_use::kv_it_value(it, 0, nullptr, 0);
        check(size >= 0, "iterator read failure");
        std::vector<char> bin(size);
        check(internal_use_do_not_use::kv_it_value(it, 0, bin.data(), size) == size, "iterator read failure");
        if (!ind->is_primary) {
            auto temp_it = ind->t->primary_index->get_temp_it();
            internal_use_do_not_use::kv_it_lower_bound(temp_it, bin.data(), bin.size());
            check(internal_use_do_not_use::kv_it_key_matches(temp_it, bin.data(), bin.size()), "iterator read failure");
            size = internal_use_do_not_use::kv_it_value(temp_it, 0, nullptr, 0);
            check(size >= 0, "iterator read failure");
            bin.resize(size);
            check(internal_use_do_not_use::kv_it_value(temp_it, 0, bin.data(), size) == size, "iterator read failure");
        }
        obj = std::make_unique<T>();
        std::string          error;
        abieos::input_buffer b{bin.data(), bin.data() + bin.size()};
        check(abieos::bin_to_native<T>(*obj, error, b), error);
        return *obj;
    }

    // If object is not already in cache then read and return it. Returns existing cached object if present.
    // Caution 1: this may return stale value if something else changed the object.
    // Caution 2: object gets destroyed when iterator is destroyed or moved or if read_fresh() is called after this.
    const T& get() {
        if (!obj)
            read_fresh();
        return *obj;
    }

    proxy operator*() { return {*this}; }

  private:
    index*             ind = {};
    uint32_t           it  = 0;
    std::unique_ptr<T> obj;

    table_iterator(index* ind, uint32_t it)
        : ind{ind}
        , it{it} {}
};

template <typename T>
template <typename... Indexes>
void table<T>::init(abieos::name table_context, abieos::name table_name, index& primary_index, Indexes&... secondary_indexes) {
    check(!initialized, "table is already initialized");

    this->primary_index     = &primary_index;
    this->secondary_indexes = {&secondary_indexes...};

    native_to_key(table_context, prefix);
    native_to_key(table_name, prefix);
    primary_index.initialize(this, true);
    (secondary_indexes.initialize(this, false), ...);
    initialized = true;
}

template <typename T>
void table<T>::insert(const T& obj) {
    // todo: disallow overwrite
    auto pk = primary_index->get_key(obj);
    pk.insert(pk.begin(), primary_index->prefix.begin(), primary_index->prefix.end());
    internal_use_do_not_use::kv_set(pk, abieos::native_to_bin(obj));
    for (auto* ind : secondary_indexes) {
        auto sk = ind->get_key(obj);
        sk.insert(sk.begin(), ind->prefix.begin(), ind->prefix.end());
        sk.insert(sk.end(), pk.begin(), pk.end());
        // todo: re-encode the key to make pk extractable and make value empty
        internal_use_do_not_use::kv_set(sk, pk);
    }
}

template <typename T>
table_iterator<T> table<T>::begin() {
    check(initialized, "table is not initialized");
    return primary_index->begin();
}

template <typename T>
table_iterator<T> table<T>::end() {
    check(initialized, "table is not initialized");
    return primary_index->end();
}

template <typename T>
void index<T>::initialize(table* t, bool is_primary) {
    this->t          = t;
    this->is_primary = is_primary;
    prefix           = t->prefix;
    native_to_key(index_name, prefix);
}

template <typename T>
table_iterator<T> index<T>::begin() {
    check(t, "index is not in a table");
    iterator result{this, internal_use_do_not_use::kv_it_create(prefix.data(), prefix.size())};
    internal_use_do_not_use::kv_it_move_to_begin(result.it);
    return result;
}

template <typename T>
table_iterator<T> index<T>::end() {
    check(t, "index is not in a table");
    return {this, 0};
};

template <typename T>
const T& table_proxy<T>::get() {
    return it.get();
}

} // namespace eosio
