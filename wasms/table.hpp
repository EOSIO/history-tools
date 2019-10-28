#pragma once

#include <abieos.hpp>
#include <type_traits>

#ifdef EOSIO_CDT_COMPILATION
#include <eosio/name.hpp>
#endif

namespace abieos {

template <typename T>
inline constexpr bool serial_reversible = std::is_signed_v<T> || std::is_unsigned_v<T>;

template <>
inline constexpr bool serial_reversible<abieos::name> = true;

#ifdef EOSIO_CDT_COMPILATION
template <>
inline constexpr bool serial_reversible<eosio::name> = true;
#endif

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

inline void native_to_key(const std::string& obj, std::vector<char>& bin) {
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

#ifdef EOSIO_CDT_COMPILATION

#define IMPORT extern "C" __attribute__((eosio_wasm_import))

IMPORT void kv_set(const char* k_begin, uint32_t k_size, const char* v_begin, uint32_t v_size);
IMPORT void kv_erase(const char* k_begin, uint32_t k_size);
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

#undef IMPORT

#endif

} // namespace internal_use_do_not_use

template <typename T>
class table;

template <typename T>
class index;

template <typename T>
class table_iterator;

template <typename T>
class table_proxy;

#ifdef EOSIO_CDT_COMPILATION
class kv_environment {
  public:
    kv_environment() {}

  private:
    void kv_set(const std::vector<char>& k, const std::vector<char>& v) {
        internal_use_do_not_use::kv_set(k.data(), k.size(), v.data(), v.size());
    }

    // clang-format off
    void     kv_erase(const char* k_begin, uint32_t k_size)                             {return internal_use_do_not_use::kv_erase(k_begin,  k_size);}
    uint32_t kv_it_create(const char* prefix, uint32_t size)                            {return internal_use_do_not_use::kv_it_create(prefix, size);}
    void     kv_it_destroy(uint32_t index)                                              {return internal_use_do_not_use::kv_it_destroy(index);}
    bool     kv_it_is_end(uint32_t index)                                               {return internal_use_do_not_use::kv_it_is_end(index);}
    int      kv_it_compare(uint32_t a, uint32_t b)                                      {return internal_use_do_not_use::kv_it_compare(a, b);}
    bool     kv_it_move_to_begin(uint32_t index)                                        {return internal_use_do_not_use::kv_it_move_to_begin(index);}
    void     kv_it_move_to_end(uint32_t index)                                          {return internal_use_do_not_use::kv_it_move_to_end(index);}
    void     kv_it_lower_bound(uint32_t index, const char* key, uint32_t size)          {return internal_use_do_not_use::kv_it_lower_bound(index, key, size);}
    bool     kv_it_key_matches(uint32_t index, const char* key, uint32_t size)          {return internal_use_do_not_use::kv_it_key_matches(index, key, size);}
    bool     kv_it_incr(uint32_t index)                                                 {return internal_use_do_not_use::kv_it_incr(index);}
    int32_t  kv_it_key(uint32_t index, uint32_t offset, char* dest, uint32_t size)      {return internal_use_do_not_use::kv_it_key(index, offset, dest, size);}
    int32_t  kv_it_value(uint32_t index, uint32_t offset, char* dest, uint32_t size)    {return internal_use_do_not_use::kv_it_value(index, offset, dest, size);}
    // clang-format on

    template <typename T>
    friend class table;

    template <typename T>
    friend class index;

    template <typename T>
    friend class table_iterator;

    template <typename T>
    friend class table_proxy;
};
#endif

struct default_constructor_tag;

template <typename T>
class table {
  public:
    using value_type = T;
    using index      = eosio::index<T>;
    using iterator   = table_iterator<T>;
    using proxy      = table_proxy<T>;
    friend index;
    friend iterator;
    friend proxy;

    kv_environment environment;

    template <typename KVE = kv_environment>
    table(typename std::enable_if_t<std::is_constructible_v<KVE>, default_constructor_tag>* = nullptr) {}

    table(kv_environment environment)
        : environment{std::move(environment)} {}

    table(const table&) = delete;
    table(table&&)      = delete;

    template <typename... Indexes>
    void init(abieos::name table_context, abieos::name table_name, index& primary_index, Indexes&... secondary_indexes);

    void     insert(const T& obj, bool bypass_preexist_check = false);
    void     erase(const T& obj);
    iterator begin();
    iterator end();

  private:
    bool                initialized{};
    std::vector<char>   prefix{};
    index*              primary_index{};
    std::vector<index*> secondary_indexes{};

    void erase_pk(const std::vector<char>& pk);
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
            t->environment.kv_it_destroy(temp_it);
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
            temp_it = t->environment.kv_it_create(prefix.data(), prefix.size());
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
            ind->t->environment.kv_it_destroy(it);
    }

    table_iterator& operator=(const table_iterator&) = delete;
    table_iterator& operator=(table_iterator&&) = default;

    friend int compare(const table_iterator& a, const table_iterator& b) {
        bool a_end = !a.it || a.ind->t->environment.kv_it_is_end(a.it);
        bool b_end = !b.it || a.ind->t->environment.kv_it_is_end(b.it);
        if (a_end && b_end)
            return 0;
        else if (a_end && !b_end)
            return 1;
        else if (!a_end && b_end)
            return -1;
        else
            return a.ind->t->environment.kv_it_compare(a.it, b.it);
    }

    friend bool operator==(const table_iterator& a, const table_iterator& b) { return compare(a, b) == 0; }
    friend bool operator!=(const table_iterator& a, const table_iterator& b) { return compare(a, b) != 0; }
    friend bool operator<(const table_iterator& a, const table_iterator& b) { return compare(a, b) < 0; }
    friend bool operator<=(const table_iterator& a, const table_iterator& b) { return compare(a, b) <= 0; }
    friend bool operator>(const table_iterator& a, const table_iterator& b) { return compare(a, b) > 0; }
    friend bool operator>=(const table_iterator& a, const table_iterator& b) { return compare(a, b) >= 0; }

    table_iterator& operator++() {
        if (it)
            ind->t->environment.kv_it_incr(it);
        obj.reset();
        return *this;
    }

    std::vector<char> get_raw_key() {
        auto size = ind->t->environment.kv_it_key(it, 0, nullptr, 0);
        check(size >= 0, "iterator read failure");
        std::vector<char> result(size);
        check(ind->t->environment.kv_it_key(it, 0, result.data(), size) == size, "iterator read failure");
        return result;
    }

    std::vector<char> get_raw_value() {
        auto size = ind->t->environment.kv_it_value(it, 0, nullptr, 0);
        check(size >= 0, "iterator read failure");
        std::vector<char> result(size);
        check(ind->t->environment.kv_it_value(it, 0, result.data(), size) == size, "iterator read failure");
        return result;
    }

    // Reads object, stores it in cache, and returns it. Use this if something else may have changed object.
    // Caution: object gets destroyed when iterator is destroyed or moved or if read_fresh() is called again.
    const T& read_fresh() {
        auto size = ind->t->environment.kv_it_value(it, 0, nullptr, 0);
        check(size >= 0, "iterator read failure");
        std::vector<char> bin(size);
        check(ind->t->environment.kv_it_value(it, 0, bin.data(), size) == size, "iterator read failure");
        if (!ind->is_primary) {
            auto temp_it = ind->t->primary_index->get_temp_it();
            ind->t->environment.kv_it_lower_bound(temp_it, bin.data(), bin.size());
            check(ind->t->environment.kv_it_key_matches(temp_it, bin.data(), bin.size()), "iterator read failure");
            size = ind->t->environment.kv_it_value(temp_it, 0, nullptr, 0);
            check(size >= 0, "iterator read failure");
            bin.resize(size);
            check(ind->t->environment.kv_it_value(temp_it, 0, bin.data(), size) == size, "iterator read failure");
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
void table<T>::insert(const T& obj, bool bypass_preexist_check) {
    auto pk = primary_index->get_key(obj);
    pk.insert(pk.begin(), primary_index->prefix.begin(), primary_index->prefix.end());
    if (!bypass_preexist_check)
        erase_pk(pk);
    environment.kv_set(pk, abieos::native_to_bin(obj));
    for (auto* ind : secondary_indexes) {
        auto sk = ind->get_key(obj);
        sk.insert(sk.begin(), ind->prefix.begin(), ind->prefix.end());
        sk.insert(sk.end(), pk.begin(), pk.end());
        // todo: re-encode the key to make pk extractable and make value empty
        environment.kv_set(sk, pk);
    }
}

template <typename T>
void table<T>::erase(const T& obj) {
    auto pk = primary_index->get_key(obj);
    pk.insert(pk.begin(), primary_index->prefix.begin(), primary_index->prefix.end());
    erase_pk(pk);
}

template <typename T>
void table<T>::erase_pk(const std::vector<char>& pk) {
    auto temp_it = primary_index->get_temp_it();
    environment.kv_it_lower_bound(temp_it, pk.data(), pk.size());
    if (!environment.kv_it_key_matches(temp_it, pk.data(), pk.size()))
        return;
    auto size = environment.kv_it_value(temp_it, 0, nullptr, 0);
    check(size >= 0, "iterator read failure");
    std::vector<char> bin(size);
    check(environment.kv_it_value(temp_it, 0, bin.data(), size) == size, "iterator read failure");
    T                    obj;
    std::string          error;
    abieos::input_buffer b{bin.data(), bin.data() + bin.size()};
    check(abieos::bin_to_native<T>(obj, error, b), error);
    for (auto* ind : secondary_indexes) {
        auto sk = ind->get_key(obj);
        sk.insert(sk.begin(), ind->prefix.begin(), ind->prefix.end());
        sk.insert(sk.end(), pk.begin(), pk.end());
        environment.kv_erase(sk.data(), sk.size());
    }
    environment.kv_erase(pk.data(), pk.size());
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
    iterator result{this, t->environment.kv_it_create(prefix.data(), prefix.size())};
    t->environment.kv_it_move_to_begin(result.it);
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
