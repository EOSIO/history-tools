// copyright defined in LICENSE.txt

#pragma once

#include "abieos.hpp"
#include <boost/filesystem.hpp>
#include <chain_kv/chain_kv.hpp>
#include <fc/exception/exception.hpp>
#include <rocksdb/db.h>
#include <rocksdb/table.h>

namespace state_history {
namespace rdb {

template <typename T>
inline T* addr(T&& x) {
    return &x;
}

inline bool exists(chain_kv::database& db, rocksdb::Slice key) {
    rocksdb::PinnableSlice v;
    auto                   stat = db.rdb->Get(rocksdb::ReadOptions(), db.rdb->DefaultColumnFamily(), key, &v);
    if (stat.IsNotFound())
        return false;
    chain_kv::check(stat, "exists: ");
    return true;
}

struct db_view_state {
    chain_kv::view&                                        view;
    std::vector<std::shared_ptr<chain_kv::view::iterator>> iterators;
    std::vector<char>                                      kv_get_storage;

    db_view_state(chain_kv::view& view)
        : view{view}
        , iterators(1) {}

    void reset() {
        iterators.resize(1);
        view.discard_changes();
    }

    void write_and_reset() {
        iterators.resize(1);
        view.write_changes();
    }
};

template <typename Derived>
struct db_callbacks {
    Derived& derived() { return static_cast<Derived&>(*this); }

    int32_t kv_get(const char* k_begin, uint32_t k_size) {
        derived().check_bounds(k_begin, k_size);
        if (!derived().state.view.get({k_begin, k_size}, derived().state.kv_get_storage))
            return -1;
        return derived().state.kv_get_storage.size();
    }

    int32_t kv_get_data(char* v_begin, uint32_t v_size) {
        derived().check_bounds(v_begin, v_size);
        auto& storage = derived().state.kv_get_storage;
        memcpy(v_begin, storage.data(), std::min((size_t)v_size, storage.size()));
        return storage.size();
    }

    void kv_set(const char* k_begin, uint32_t k_size, const char* v_begin, uint32_t v_size) {
        derived().check_bounds(k_begin, k_size);
        derived().check_bounds(v_begin, v_size);
        derived().state.view.set({k_begin, k_size}, {v_begin, v_size});
    }

    void kv_erase(const char* k_begin, uint32_t k_size) {
        derived().check_bounds(k_begin, k_size);
        derived().state.view.erase({k_begin, k_size});
    }

    chain_kv::view::iterator& get_it(uint32_t index) {
        auto& state = derived().state;
        if (index >= state.iterators.size() || !state.iterators[index])
            throw std::runtime_error("iterator does not exist");
        return *state.iterators[index];
    }

    uint32_t kv_it_create(const char* prefix, uint32_t size) {
        // todo: reuse destroyed slots?
        auto& state = derived().state;
        state.iterators.push_back(std::make_unique<chain_kv::view::iterator>(state.view, std::vector<char>{prefix, prefix + size}));
        return state.iterators.size() - 1;
    }

    void kv_it_destroy(uint32_t index) {
        get_it(index);
        derived().state.iterators[index].reset();
    }

    bool kv_it_is_end(uint32_t index) { return get_it(index).is_end(); }

    int kv_it_compare(uint32_t a, uint32_t b) { return compare(get_it(a), get_it(b)); }

    bool kv_it_move_to_begin(uint32_t index) {
        auto& it = get_it(index);
        it.move_to_begin();
        return !it.is_end();
    }

    void kv_it_move_to_end(uint32_t index) {
        auto& it = get_it(index);
        it.move_to_end();
    }

    void kv_it_lower_bound(uint32_t index, const char* key, uint32_t size) {
        derived().check_bounds(key, size);
        auto& it = get_it(index);
        it.lower_bound(key, size);
    }

    // todo: kv_it_key_compare instead?
    bool kv_it_key_matches(uint32_t index, const char* key, uint32_t size) {
        derived().check_bounds(key, size);
        auto& it = get_it(index);
        auto  kv = it.get_kv();
        return kv && kv->key.size() == size && !memcmp(kv->key.data(), key, size);
    }

    bool kv_it_incr(uint32_t index) {
        auto& it = get_it(index);
        ++it;
        return !it.is_end();
    }

    int32_t kv_it_key(uint32_t index, uint32_t offset, char* dest, uint32_t size) {
        derived().check_bounds(dest, size);
        if (offset)
            throw std::runtime_error("offset must be 0");
        auto& it = get_it(index);
        auto  kv = it.get_kv();
        if (!kv)
            return -1;
        auto actual_size = kv->key.size();
        if (actual_size >= 0x8000'0000)
            throw std::runtime_error("kv size is too big for wasm");
        memcpy(dest, kv->key.data(), std::min(size, (uint32_t)actual_size));
        return actual_size;
    }

    int32_t kv_it_value(uint32_t index, uint32_t offset, char* dest, uint32_t size) {
        derived().check_bounds(dest, size);
        if (offset)
            throw std::runtime_error("offset must be 0");
        auto& it = get_it(index);
        auto  kv = it.get_kv();
        if (!kv)
            return -1;
        if (kv->value.size() >= 0x8000'0000)
            throw std::runtime_error("kv size is too big for wasm");
        memcpy(dest, kv->value.data(), std::min(size, (uint32_t)kv->value.size()));
        return kv->value.size();
    }

    template <typename Rft, typename Allocator>
    static void register_callbacks() {
        Rft::template add<Derived, &Derived::kv_get, Allocator>("env", "kv_get");
        Rft::template add<Derived, &Derived::kv_get_data, Allocator>("env", "kv_get_data");
        Rft::template add<Derived, &Derived::kv_set, Allocator>("env", "kv_set");
        Rft::template add<Derived, &Derived::kv_erase, Allocator>("env", "kv_erase");
        Rft::template add<Derived, &Derived::kv_it_create, Allocator>("env", "kv_it_create");
        Rft::template add<Derived, &Derived::kv_it_destroy, Allocator>("env", "kv_it_destroy");
        Rft::template add<Derived, &Derived::kv_it_is_end, Allocator>("env", "kv_it_is_end");
        Rft::template add<Derived, &Derived::kv_it_compare, Allocator>("env", "kv_it_compare");
        Rft::template add<Derived, &Derived::kv_it_move_to_begin, Allocator>("env", "kv_it_move_to_begin");
        Rft::template add<Derived, &Derived::kv_it_move_to_end, Allocator>("env", "kv_it_move_to_end");
        Rft::template add<Derived, &Derived::kv_it_lower_bound, Allocator>("env", "kv_it_lower_bound");
        Rft::template add<Derived, &Derived::kv_it_key_matches, Allocator>("env", "kv_it_key_matches");
        Rft::template add<Derived, &Derived::kv_it_incr, Allocator>("env", "kv_it_incr");
        Rft::template add<Derived, &Derived::kv_it_key, Allocator>("env", "kv_it_key");
        Rft::template add<Derived, &Derived::kv_it_value, Allocator>("env", "kv_it_value");
    }
}; // db_callbacks

class kv_environment : public db_callbacks<kv_environment> {
  public:
    using base = db_callbacks<kv_environment>;
    db_view_state& state;

    kv_environment(db_view_state& state)
        : state{state} {}
    kv_environment(const kv_environment&) = default;

    void check_bounds(const char*, uint32_t) {}
    void kv_set(const std::vector<char>& k, const std::vector<char>& v) { base::kv_set(k.data(), k.size(), v.data(), v.size()); }
};

} // namespace rdb
} // namespace state_history

namespace eosio {
using state_history::rdb::kv_environment;

inline void check(bool cond, const char* msg) {
    if (!cond)
        throw std::runtime_error(msg);
}

inline void check(bool cond, const std::string& msg) {
    if (!cond)
        throw std::runtime_error(msg);
}

} // namespace eosio
