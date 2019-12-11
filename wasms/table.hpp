#pragma once

#include <abieos.hpp>
#include <eosio/to_key.hpp>
#include <type_traits>

#ifdef EOSIO_CDT_COMPILATION
#include <eosio/name.hpp>
#endif

namespace abieos {

template <typename S>
eosio::result<void> to_key(const name& obj, S& stream) {
    return to_key(obj.value, stream);
}

} // namespace abieos

namespace eosio {
namespace internal_use_do_not_use {

#ifdef EOSIO_CDT_COMPILATION

#define IMPORT extern "C" __attribute__((eosio_wasm_import))

// clang-format off
IMPORT void     kv_erase(uint64_t db, uint64_t contract, const char* key, uint32_t key_size);
IMPORT void     kv_set(uint64_t db, uint64_t contract, const char* key, uint32_t key_size, const char* value, uint32_t value_size);
IMPORT bool     kv_get(uint64_t db, uint64_t contract, const char* key, uint32_t key_size, uint32_t& value_size);
IMPORT uint32_t kv_get_data(uint64_t db, uint32_t offset, char* data, uint32_t data_size);
IMPORT uint32_t kv_it_create(uint64_t db, uint64_t contract, const char* prefix, uint32_t size);
IMPORT void     kv_it_destroy(uint32_t itr);
IMPORT it_stat  kv_it_status(uint32_t itr);
IMPORT int      kv_it_compare(uint32_t itr_a, uint32_t itr_b);
IMPORT int      kv_it_key_compare(uint32_t itr, const char* key, uint32_t size);
IMPORT it_stat  kv_it_move_to_end(uint32_t itr);
IMPORT it_stat  kv_it_next(uint32_t itr);
IMPORT it_stat  kv_it_prev(uint32_t itr);
IMPORT it_stat  kv_it_lower_bound(uint32_t itr, const char* key, uint32_t size);
IMPORT it_stat  kv_it_key(uint32_t itr, uint32_t offset, char* dest, uint32_t size, uint32_t& actual_size);
IMPORT it_stat  kv_it_value(uint32_t itr, uint32_t offset, char* dest, uint32_t size, uint32_t& actual_size);
// clang-format on

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
    void     kv_erase(uint64_t db, uint64_t contract, const char* key, uint32_t key_size)                                       {return internal_use_do_not_use::kv_erase(db, contract, key, key_size);}
    void     kv_set(uint64_t db, uint64_t contract, const char* key, uint32_t key_size, const char* value, uint32_t value_size) {return internal_use_do_not_use::kv_set(db, contract, key, key_size, value, value_size);}
    bool     kv_get(uint64_t db, uint64_t contract, const char* key, uint32_t key_size, uint32_t& value_size)                   {return internal_use_do_not_use::kv_get(db, contract, key, key_size, value_size);}
    uint32_t kv_get_data(uint64_t db, uint32_t offset, char* data, uint32_t data_size)                                          {return internal_use_do_not_use::kv_get_data(db, offset, data, data_size);}
    uint32_t kv_it_create(uint64_t db, uint64_t contract, const char* prefix, uint32_t size)                                    {return internal_use_do_not_use::kv_it_create(db, contract, prefix, size);}
    void     kv_it_destroy(uint32_t itr)                                                                                        {return internal_use_do_not_use::kv_it_destroy(itr);}
    it_stat  kv_it_status(uint32_t itr)                                                                                         {return internal_use_do_not_use::kv_it_status(itr);}
    int      kv_it_compare(uint32_t itr_a, uint32_t itr_b)                                                                      {return internal_use_do_not_use::kv_it_compare(itr_a, itr_b);}
    int      kv_it_key_compare(uint32_t itr, const char* key, uint32_t size)                                                    {return internal_use_do_not_use::kv_it_key_compare(itr, key, size);}
    it_stat  kv_it_move_to_end(uint32_t itr)                                                                                    {return internal_use_do_not_use::kv_it_move_to_end(itr);}
    it_stat  kv_it_next(uint32_t itr)                                                                                           {return internal_use_do_not_use::kv_it_next(itr);}
    it_stat  kv_it_prev(uint32_t itr)                                                                                           {return internal_use_do_not_use::kv_it_prev(itr);}
    it_stat  kv_it_lower_bound(uint32_t itr, const char* key, uint32_t size)                                                    {return internal_use_do_not_use::kv_it_lower_bound(itr, key, size);}
    it_stat  kv_it_key(uint32_t itr, uint32_t offset, char* dest, uint32_t size, uint32_t& actual_size)                         {return internal_use_do_not_use::kv_it_key(itr, offset, dest, size, actual_size);}
    it_stat  kv_it_value(uint32_t itr, uint32_t offset, char* dest, uint32_t size, uint32_t& actual_size)                       {return internal_use_do_not_use::kv_it_value(itr, offset, dest, size, actual_size);}
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
    void init(abieos::name database, abieos::name contract, abieos::name table_name, index& primary_index, Indexes&... secondary_indexes);

    void     insert(const T& obj, bool bypass_preexist_check = false);
    void     erase(const T& obj);
    iterator begin();
    iterator end();

  private:
    bool                initialized{};
    abieos::name        database{};
    abieos::name        contract{};
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
            temp_it = t->environment.kv_it_create(t->database.value, t->contract.value, prefix.data(), prefix.size());
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
        if (!a.it && !b.it)
            return 0;
        else if (!a.it && b.it)
            return 1;
        else if (a.it && !b.it)
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
        uint32_t size;
        check(!ind->t->environment.kv_it_key(it, 0, nullptr, 0, size), "iterator read failure");
        std::vector<char> result(size);
        check(!ind->t->environment.kv_it_key(it, 0, result.data(), size, size), "iterator read failure");
        return result;
    }

    std::vector<char> get_raw_value() {
        uint32_t size;
        check(!ind->t->environment.kv_it_value(it, 0, nullptr, 0, size), "iterator read failure");
        std::vector<char> result(size);
        check(!ind->t->environment.kv_it_value(it, 0, result.data(), size, size), "iterator read failure");
        return result;
    }

    // Reads object, stores it in cache, and returns it. Use this if something else may have changed object.
    // Caution: object gets destroyed when iterator is destroyed or moved or if read_fresh() is called again.
    const T& read_fresh() {
        uint32_t size;
        check(!ind->t->environment.kv_it_value(it, 0, nullptr, 0, size), "iterator read failure");
        std::vector<char> bin(size);
        check(!ind->t->environment.kv_it_value(it, 0, bin.data(), size, size), "iterator read failure");
        if (!ind->is_primary) {
            // !!!
            auto temp_it = ind->t->primary_index->get_temp_it();
            ind->t->environment.kv_it_lower_bound(temp_it, bin.data(), bin.size());
            check(!ind->t->environment.kv_it_key_compare(temp_it, bin.data(), bin.size()), "iterator read failure");
            check(!ind->t->environment.kv_it_value(temp_it, 0, nullptr, 0, size), "iterator read failure");
            bin.resize(size);
            check(!ind->t->environment.kv_it_value(temp_it, 0, bin.data(), size, size), "iterator read failure");
        }
        obj = std::make_unique<T>();
        check_discard(convert_from_bin(*obj, bin));
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
void table<T>::init(
    abieos::name database, abieos::name contract, abieos::name table_name, index& primary_index, Indexes&... secondary_indexes) {
    check(!initialized, "table is already initialized");

    this->database          = database;
    this->contract          = contract;
    this->primary_index     = &primary_index;
    this->secondary_indexes = {&secondary_indexes...};

    prefix.reserve(16);
    vector_stream stream{prefix};
    (void)to_key(contract, stream); // !!!
    (void)to_key(table_name, stream);
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
    environment.kv_set(database.value, contract.value, pk, check(convert_to_bin(obj)).value());
    for (auto* ind : secondary_indexes) {
        auto sk = ind->get_key(obj);
        sk.insert(sk.begin(), ind->prefix.begin(), ind->prefix.end());
        sk.insert(sk.end(), pk.begin(), pk.end());
        // todo: re-encode the key to make pk extractable and make value empty
        environment.kv_set(database.value, contract.value, sk, pk);
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
    if (environment.kv_it_key_compare(temp_it, pk.data(), pk.size()))
        return;
    uint32_t size;
    // !!! check return value
    environment.kv_it_value(temp_it, 0, nullptr, 0, size);
    check(size >= 0, "iterator read failure");
    std::vector<char> bin(size);
    // !!! check return value
    environment.kv_it_value(temp_it, 0, bin.data(), size, size);
    T obj;
    check_discard(convert_from_bin(obj, bin));
    for (auto* ind : secondary_indexes) {
        auto sk = ind->get_key(obj);
        sk.insert(sk.begin(), ind->prefix.begin(), ind->prefix.end());
        sk.insert(sk.end(), pk.begin(), pk.end());
        environment.kv_erase(database.value, contract.value, sk.data(), sk.size());
    }
    environment.kv_erase(database.value, contract.value, pk.data(), pk.size());
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

    prefix.reserve(8);
    vector_stream stream{prefix};
    (void)to_key(index_name, stream);
}

template <typename T>
table_iterator<T> index<T>::begin() {
    check(t, "index is not in a table");
    iterator result{this, t->environment.kv_it_create(t->database.value, t->contract.value, prefix.data(), prefix.size())};
    // !!! check return ???
    t->environment.kv_it_move_to_end(result.it);
    // !!! check return ???
    t->environment.kv_it_next(result.it);
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
