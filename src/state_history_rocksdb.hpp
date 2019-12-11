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
    std::vector<char>                                      temp_data_buffer; // !!! per db

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

    void kv_erase(uint64_t db, uint64_t contract, const char* key, uint32_t key_size) {
        derived().check_bounds(key, key_size);
        derived().state.view.erase({key, key_size});
        derived().state.temp_data_buffer.clear();
    }

    void kv_set(uint64_t db, uint64_t contract, const char* key, uint32_t key_size, const char* value, uint32_t value_size) {
        // !!! db, contract
        // !!! limit key, value sizes
        derived().check_bounds(key, key_size);
        derived().check_bounds(value, value_size);
        derived().state.view.set({key, key_size}, {value, value_size});
        derived().state.temp_data_buffer.clear();
    }

    bool kv_get(uint64_t db, uint64_t contract, const char* key, uint32_t key_size, uint32_t& value_size) {
        // !!! db, contract
        derived().check_bounds(key, key_size);
        bool result = derived().state.view.get({key, key_size}, derived().state.temp_data_buffer);
        value_size  = derived().state.temp_data_buffer.size();
        return result;
    }

    uint32_t kv_get_data(uint64_t db, uint32_t offset, char* data, uint32_t data_size) {
        // !!! db
        derived().check_bounds(data, data_size);
        auto& temp = derived().state.temp_data_buffer;
        if (offset < temp.size())
            memcpy(data, temp.data() + offset, std::min(data_size, temp.size() - offset));
        return temp.size();
    }

    chain_kv::view::iterator& get_it(uint32_t itr) {
        auto& state = derived().state;
        if (itr >= state.iterators.size() || !state.iterators[itr])
            throw std::runtime_error("iterator does not exist");
        return *state.iterators[itr];
    }

    uint32_t kv_it_create(uint64_t db, uint64_t contract, const char* prefix, uint32_t size) {
        // !!! db, contract
        // !!! reuse destroyed slots
        // !!! limit # of iterators
        auto& state = derived().state;
        state.iterators.push_back(std::make_unique<chain_kv::view::iterator>(state.view, std::vector<char>{prefix, prefix + size}));
        return state.iterators.size() - 1;
    }

    void kv_it_destroy(uint32_t itr) {
        get_it(itr);
        derived().state.iterators[itr].reset();
    }

    int32_t kv_it_status(uint32_t itr) {
        // !!!
        throw std::runtime_error("not implemented");
    }

    int kv_it_compare(uint32_t itr_a, uint32_t itr_b) {
        // !!! throw if either is erased
        // !!! throw if from different db or contracts
        return compare(get_it(itr_a), get_it(itr_b));
    }

    int kv_it_key_compare(uint32_t itr, const char* key, uint32_t size) {
        // !!! throw if itr is erased
        derived().check_bounds(key, size);
        auto& it = get_it(itr);
        auto  kv = it.get_kv();
        if (!kv)
            return 1;
        return chain_kv::compare_blob(kv->key, std::string_view{key, size});
    }

    int32_t kv_it_move_to_end(uint32_t itr) {
        auto& it = get_it(itr);
        it.move_to_end();
        return 0; // !!!
    }

    int32_t kv_it_next(uint32_t itr) {
        // !!! throw if itr is erased
        // !!! wraparound
        auto& it = get_it(itr);
        ++it;
        return 0; // !!!
    }

    int32_t kv_it_prev(uint32_t itr) {
        // !!!
        throw std::runtime_error("not implemented");
    }

    int32_t kv_it_lower_bound(uint32_t itr, const char* key, uint32_t size) {
        // !!! semantics change
        derived().check_bounds(key, size);
        auto& it = get_it(itr);
        it.lower_bound(key, size);
        return 0; // !!!
    }

    int32_t kv_it_key(uint32_t itr, uint32_t offset, char* dest, uint32_t size, uint32_t& actual_size) {
        // !!! throw if itr is erased
        derived().check_bounds(dest, size);
        auto& it = get_it(itr);
        auto  kv = it.get_kv();
        if (!kv) {
            actual_size = 0;
            return -1; // !!!
        }
        if (kv->key.size() >= 0x8000'0000)
            throw std::runtime_error("kv size is too big for wasm");
        if (offset < kv->key.size())
            memcpy(dest, kv->key.data() + offset, std::min(size, kv->key.size() - offset));
        actual_size = kv->key.size();
        return 0; // !!!
    }

    int32_t kv_it_value(uint32_t itr, uint32_t offset, char* dest, uint32_t size, uint32_t& actual_size) {
        // !!! throw if itr is erased
        derived().check_bounds(dest, size);
        auto& it = get_it(itr);
        auto  kv = it.get_kv();
        if (!kv) {
            actual_size = 0;
            return -1; // !!!
        }
        if (kv->value.size() >= 0x8000'0000)
            throw std::runtime_error("kv size is too big for wasm");
        if (offset < kv->value.size())
            memcpy(dest, kv->value.data() + offset, std::min(size, (uint32_t)kv->value.size() - offset));
        actual_size = kv->value.size();
        return 0; // !!!
    }

    template <typename Rft, typename Allocator>
    static void register_callbacks() {
        Rft::template add<Derived, &Derived::kv_erase, Allocator>("env", "kv_erase");
        Rft::template add<Derived, &Derived::kv_set, Allocator>("env", "kv_set");
        Rft::template add<Derived, &Derived::kv_get, Allocator>("env", "kv_get");
        Rft::template add<Derived, &Derived::kv_get_data, Allocator>("env", "kv_get_data");
        Rft::template add<Derived, &Derived::kv_it_create, Allocator>("env", "kv_it_create");
        Rft::template add<Derived, &Derived::kv_it_destroy, Allocator>("env", "kv_it_destroy");
        Rft::template add<Derived, &Derived::kv_it_status, Allocator>("env", "kv_it_status");
        Rft::template add<Derived, &Derived::kv_it_compare, Allocator>("env", "kv_it_compare");
        Rft::template add<Derived, &Derived::kv_it_key_compare, Allocator>("env", "kv_it_key_compare");
        Rft::template add<Derived, &Derived::kv_it_move_to_end, Allocator>("env", "kv_it_move_to_end");
        Rft::template add<Derived, &Derived::kv_it_next, Allocator>("env", "kv_it_next");
        Rft::template add<Derived, &Derived::kv_it_prev, Allocator>("env", "kv_it_prev");
        Rft::template add<Derived, &Derived::kv_it_lower_bound, Allocator>("env", "kv_it_lower_bound");
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

    void kv_set(uint64_t db, uint64_t contract, const std::vector<char>& k, const std::vector<char>& v) {
        base::kv_set(db, contract, k.data(), k.size(), v.data(), v.size());
    }
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
