// copyright defined in LICENSE.txt

#pragma once

#include "abieos.hpp"
#include <boost/filesystem.hpp>
#include <fc/exception/exception.hpp>
#include <rocksdb/db.h>
#include <rocksdb/table.h>

namespace state_history {
namespace rdb {

template <typename T>
inline T* addr(T&& x) {
    return &x;
}

inline void check(rocksdb::Status s, const char* prefix) {
    if (!s.ok())
        throw std::runtime_error(std::string(prefix) + s.ToString());
}

struct database {
    std::shared_ptr<rocksdb::Statistics> stats;
    std::unique_ptr<rocksdb::DB>         db;

    database(const char* db_path, std::optional<uint32_t> threads, std::optional<uint32_t> max_open_files, bool fast_reads) {
        rocksdb::DB*     p;
        rocksdb::Options options;
        // stats = options.statistics = rocksdb::CreateDBStatistics();
        // stats->set_stats_level(rocksdb::kExceptTimeForMutex);
        // options.stats_dump_period_sec = 2;
        options.create_if_missing = true;

        options.level_compaction_dynamic_level_bytes = true;
        options.max_background_compactions           = 4;
        options.max_background_flushes               = 2;
        options.bytes_per_sync                       = 1048576;
        options.compaction_pri                       = rocksdb::kMinOverlappingRatio;

        if (threads)
            options.IncreaseParallelism(*threads);
        options.OptimizeLevelStyleCompaction(256ull << 20);
        for (auto& x : options.compression_per_level) // todo: fix snappy build
            x = rocksdb::kNoCompression;

        if (fast_reads) {
            ilog("open ${p}: fast reader mode; writes will be slower", ("p", db_path));
        } else {
            ilog("open ${p}: fast writer mode", ("p", db_path));
            options.memtable_factory                = std::make_shared<rocksdb::VectorRepFactory>();
            options.allow_concurrent_memtable_write = false;
        }
        if (max_open_files)
            options.max_open_files = *max_open_files;

        rocksdb::BlockBasedTableOptions table_options;
        table_options.format_version               = 4;
        table_options.index_block_restart_interval = 16;
        // table_options.data_block_index_type        = rocksdb::BlockBasedTableOptions::kDataBlockBinaryAndHash;
        options.table_factory.reset(NewBlockBasedTableFactory(table_options));

        check(rocksdb::DB::Open(options, db_path, &p), "rocksdb::DB::Open: ");
        db.reset(p);
        ilog("database opened");
    }

    database(const database&) = delete;
    database(database&&)      = delete;
    database& operator=(const database&) = delete;
    database& operator=(database&&) = delete;

    void flush(bool allow_write_stall, bool wait) {
        rocksdb::FlushOptions op;
        op.allow_write_stall = allow_write_stall;
        op.wait              = wait;
        db->Flush(op);
    }
};

inline rocksdb::Slice to_slice(const std::vector<char>& v) { return {v.data(), v.size()}; }

inline rocksdb::Slice to_slice(eosio::input_stream v) { return {v.pos, size_t(v.end - v.pos)}; }

inline eosio::input_stream to_input_buffer(rocksdb::Slice v) { return {v.data(), v.data() + v.size()}; }

inline eosio::input_stream to_input_buffer(rocksdb::PinnableSlice& v) { return {v.data(), v.data() + v.size()}; }

inline void put(rocksdb::WriteBatch& batch, const std::vector<char>& key, const std::vector<char>& value, bool overwrite = false) {
    // !!! remove overwrite
    batch.Put(to_slice(key), to_slice(value));
}

/*
template <typename T>
void put(rocksdb::WriteBatch& batch, const std::vector<char>& key, const T& value, bool overwrite = false) {
    put(batch, key, abieos::native_to_bin(value), overwrite);
}
*/

inline void write(database& db, rocksdb::WriteBatch& batch) {
    // todo: verify status write order
    rocksdb::WriteOptions opt;
    opt.disableWAL = true;
    check(db.db->Write(opt, &batch), "write batch");
    batch.Clear();
}

inline bool exists(database& db, rocksdb::Slice key) {
    rocksdb::PinnableSlice v;
    auto                   stat = db.db->Get(rocksdb::ReadOptions(), db.db->DefaultColumnFamily(), key, &v);
    if (stat.IsNotFound())
        return false;
    check(stat, "exists: ");
    return true;
}

inline std::optional<eosio::input_stream> get_raw(rocksdb::Iterator& it, const std::vector<char>& key, bool required) {
    it.Seek(to_slice(key));
    auto stat = it.status();
    if (stat.IsNotFound() && !required)
        return {};
    check(stat, "Seek: ");
    auto k = it.key();
    if (k.size() != key.size() || memcmp(key.data(), k.data(), key.size())) {
        if (required)
            throw std::runtime_error("key not found");
        else
            return {};
    }
    return to_input_buffer(it.value());
}
/*
template <typename T>
std::optional<T> get(rocksdb::Iterator& it, const std::vector<char>& key, bool required) {
    auto bin = get_raw(it, key, required);
    if (bin)
        return abieos::bin_to_native<T>(*bin);
    else
        return {};
}
*/
/*
template <typename T>
std::optional<T> get(database& db, const std::vector<char>& key, bool required) {
    rocksdb::PinnableSlice v;
    auto                   stat = db.db->Get(rocksdb::ReadOptions(), db.db->DefaultColumnFamily(), to_slice(key), &v);
    if (stat.IsNotFound() && !required)
        return {};
    check(stat, "get: ");
    auto bin = to_input_buffer(v);
    return abieos::bin_to_native<T>(bin);
}
*/
/*
// Loop through keys in range [lower_bound, upper_bound], inclusive. lower_bound and upper_bound may
// be partial keys (prefixes). They may be different sizes. Does not skip keys with duplicate prefixes.
//
// bool f(abieos::input_buffer key, abieos::input_buffer data);
// * return true to continue loop
// * return false to break out of loop
template <typename F>
void for_each(rocksdb::Iterator& it, const std::vector<char>& lower_bound, const std::vector<char>& upper_bound, F f) {
    for (it.Seek(to_slice(lower_bound)); it.Valid(); it.Next()) {
        auto k = it.key();
        if (memcmp(k.data(), upper_bound.data(), std::min(k.size(), upper_bound.size())) > 0)
            break;
        if (!f(to_input_buffer(k), to_input_buffer(it.value())))
            return;
    }
    check(it.status(), "for_each: ");
}

template <typename F>
void for_each(database& db, const std::vector<char>& lower_bound, const std::vector<char>& upper_bound, F f) {
    std::unique_ptr<rocksdb::Iterator> it{db.db->NewIterator(rocksdb::ReadOptions())};
    for_each(*it, lower_bound, upper_bound, f);
}

// Loop through keys in range [lower_bound, upper_bound], inclusive. Skip keys with duplicate prefix.
// The prefix is the same size as lower_bound and upper_bound, which must have the same size.
//
// bool f(const std::vector& prefix, abieos::input_buffer whole_key, abieos::input_buffer data);
// * return true to continue loop
// * return false to break out of loop
template <typename F>
void for_each_subkey(rocksdb::Iterator& it, std::vector<char> lower_bound, const std::vector<char>& upper_bound, F f) {
    if (lower_bound.size() != upper_bound.size())
        throw std::runtime_error("for_each_subkey: key sizes don't match");
    it.Seek(to_slice(lower_bound));
    while (it.Valid()) {
        auto k = it.key();
        if (memcmp(k.data(), upper_bound.data(), std::min(k.size(), upper_bound.size())) > 0)
            break;
        if (k.size() < lower_bound.size())
            throw std::runtime_error("for_each_subkey: found key with size < prefix");
        memmove(lower_bound.data(), k.data(), lower_bound.size());
        if (!f(std::as_const(lower_bound), to_input_buffer(k), to_input_buffer(it.value())))
            return;
        kv::inc_key(lower_bound);
        it.Seek(to_slice(lower_bound));
    }
    check(it.status(), "for_each_subkey: ");
}

template <typename F>
void for_each_subkey(database& db, std::vector<char> lower_bound, const std::vector<char>& upper_bound, F f) {
    std::unique_ptr<rocksdb::Iterator> it{db.db->NewIterator(rocksdb::ReadOptions())};
    for_each_subkey(*it, std::move(lower_bound), upper_bound, f);
}
*/

class db_view {
  public:
    database& db;

    class iterator;
    using bytes        = std::vector<char>;
    using input_stream = eosio::input_stream;

    struct key_value {
        input_stream key   = {};
        input_stream value = {};
    };

    struct key_present_value {
        input_stream key     = {};
        bool         present = {};
        input_stream value   = {};
    };

    struct present_value {
        bool  present = {};
        bytes value   = {};
    };

  private:
    static int key_compare(input_stream a, input_stream b) {
        auto a_size = a.end - a.pos;
        auto b_size = b.end - b.pos;
        int  r      = memcmp(a.pos, b.pos, std::min(a_size, b_size));
        if (r == 0) {
            if (a_size < b_size)
                return -1;
            else if (a_size > b_size)
                return 1;
        }
        return r;
    }

    struct vector_compare {
        bool operator()(const std::vector<char>& a, const std::vector<char>& b) const {
            return key_compare({a.data(), a.data() + a.size()}, {b.data(), b.data() + b.size()});
        }
    };

    template <typename T>
    static int key_compare(const std::optional<T>& a, const std::optional<T>& b) {
        if (!a && !b)
            return 0;
        else if (!a && b)
            return 1;
        else if (a && !b)
            return -1;
        else
            return key_compare(a->key, b->key);
    }

    static const std::optional<key_present_value>&
    key_min(const std::optional<key_present_value>& a, const std::optional<key_present_value>& b) {
        auto cmp = key_compare(a, b);
        if (cmp <= 0)
            return a;
        else
            return b;
    }

    using change_map = std::map<bytes, present_value, vector_compare>;

    rocksdb::WriteBatch write_batch;
    change_map          changes;

    struct iterator_impl {
        friend db_view;
        friend iterator;

        db_view&                           view;
        std::vector<char>                  prefix;
        std::unique_ptr<rocksdb::Iterator> rocks_it;
        change_map::iterator               change_it;

        iterator_impl(db_view& view, std::vector<char> prefix)
            : view{view}
            , prefix{std::move(prefix)}
            , rocks_it{view.db.db->NewIterator(rocksdb::ReadOptions())}
            , change_it{view.changes.end()} {}

        iterator_impl(const iterator_impl&) = delete;
        iterator_impl& operator=(const iterator_impl&) = delete;

        void rocks_verify_prefix() {
            if (!rocks_it->Valid())
                return;
            auto k = rocks_it->key();
            if (k.size() >= prefix.size() && !memcmp(k.data(), prefix.data(), prefix.size()))
                return;
            rocks_it->SeekToLast();
            if (rocks_it->Valid())
                rocks_it->Next();
        }

        void changed_verify_prefix() {
            if (change_it == view.changes.end())
                return;
            auto& k = change_it->first;
            if (k.size() >= prefix.size() && !memcmp(k.data(), prefix.data(), prefix.size()))
                return;
            change_it = view.changes.end();
        }

        void move_to_begin() {
            rocks_it->Seek({prefix.data(), prefix.size()});
            rocks_verify_prefix();
            change_it = view.changes.lower_bound(prefix);
            changed_verify_prefix();
        }

        void move_to_end() {
            rocks_it->SeekToLast();
            if (rocks_it->Valid())
                rocks_it->Next();
            change_it = view.changes.end();
        }

        void lower_bound(const char* key, size_t size) {
            if (size < prefix.size() || memcmp(key, prefix.data(), prefix.size()))
                throw std::runtime_error("lower_bound: prefix doesn't match");
            rocks_it->Seek({key, size});
            rocks_verify_prefix();
            change_it = view.changes.lower_bound({key, key + size});
            changed_verify_prefix();
        }

        std::optional<key_value> get_kv() {
            auto r   = deref_rocks_it();
            auto c   = deref_change_it();
            auto min = key_min(r, c);
            if (min) {
                if (min->present)
                    return key_value{min->key, min->value};
                move_to_end(); // invalidate iterator since it is at a removed element
            }
            return {};
        }

        bool is_end() { return !get_kv(); }

        iterator_impl& operator++() {
            auto r   = deref_rocks_it();
            auto c   = deref_change_it();
            auto cmp = key_compare(r, c);
            do {
                if (cmp < 0) {
                    rocks_it->Next();
                } else if (cmp > 0) {
                    ++change_it;
                } else if (r && c) {
                    rocks_it->Next();
                    ++change_it;
                }
                r   = deref_rocks_it();
                c   = deref_change_it();
                cmp = key_compare(r, c);
            } while (cmp > 0 && !c->present);
            rocks_verify_prefix();
            changed_verify_prefix();
            return *this;
        }

        std::optional<key_present_value> deref_rocks_it() {
            if (rocks_it->Valid())
                return {{to_input_buffer(rocks_it->key()), true, to_input_buffer(rocks_it->value())}};
            else
                return {};
        }

        std::optional<key_present_value> deref_change_it() {
            if (change_it != view.changes.end())
                return {{{change_it->first.data(), change_it->first.data() + change_it->first.size()},
                         change_it->second.present,
                         {change_it->second.value.data(), change_it->second.value.data() + change_it->second.value.size()}}};
            else
                return {};
        }
    }; // iterator_impl

  public:
    class iterator {
        friend db_view;

      private:
        std::unique_ptr<iterator_impl> impl;

      public:
        iterator(db_view& view, std::vector<char> prefix)
            : impl{std::make_unique<iterator_impl>(view, std::move(prefix))} {}

        iterator(const iterator&) = delete;
        iterator(iterator&&)      = default;

        iterator& operator=(const iterator&) = delete;
        iterator& operator=(iterator&&) = default;

        friend int  compare(const iterator& a, const iterator& b) { return db_view::key_compare(a.get_kv(), b.get_kv()); }
        friend bool operator==(const iterator& a, const iterator& b) { return compare(a, b) == 0; }
        friend bool operator<(const iterator& a, const iterator& b) { return compare(a, b) < 0; }

        iterator& operator++() {
            if (impl)
                ++*impl;
            else
                throw std::runtime_error("kv iterator is not initialized");
            return *this;
        }

        void move_to_begin() {
            if (impl)
                impl->move_to_begin();
            else
                throw std::runtime_error("kv iterator is not initialized");
        }

        void move_to_end() {
            if (impl)
                impl->move_to_end();
            else
                throw std::runtime_error("kv iterator is not initialized");
        }

        void lower_bound(const char* key, size_t size) {
            if (impl)
                return impl->lower_bound(key, size);
            else
                throw std::runtime_error("kv iterator is not initialized");
        }

        void lower_bound(const std::vector<char>& key) { return lower_bound(key.data(), key.size()); }

        bool is_end() const { return !impl || impl->is_end(); }

        std::optional<key_value> get_kv() const {
            if (impl)
                return impl->get_kv();
            else
                return {};
        }
    };

    db_view(database& db)
        : db{db} {}

    void discard_changes() {
        write_batch.Clear();
        changes.clear();
    }

    void write_changes() {
        write(db, write_batch);
        discard_changes();
    }

    bool get(input_stream k, std::vector<char>& dest) {
        rocksdb::PinnableSlice v;
        auto                   stat = db.db->Get(rocksdb::ReadOptions(), db.db->DefaultColumnFamily(), to_slice(k), &v);
        if (stat.IsNotFound())
            return false;
        check(stat, "get: ");
        dest.assign(v.data(), v.data() + v.size());
        return true;
    }

    void set(input_stream k, input_stream v) {
        write_batch.Put(to_slice(k), to_slice(v));
        changes[{k.pos, k.end}] = {true, {v.pos, v.end}};
    }

    void erase(input_stream k) {
        write_batch.Delete(to_slice(k));
        changes[{k.pos, k.end}] = {false, {}};
    }
}; // db_view

struct db_view_state {
    db_view&                                        view;
    std::vector<std::shared_ptr<db_view::iterator>> iterators;
    std::vector<char>                               kv_get_storage;

    db_view_state(db_view& view)
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
        if (!derived().state.view.get({k_begin, k_begin + k_size}, derived().state.kv_get_storage))
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
        derived().state.view.set({k_begin, k_begin + k_size}, {v_begin, v_begin + v_size});
    }

    void kv_erase(const char* k_begin, uint32_t k_size) {
        derived().check_bounds(k_begin, k_size);
        derived().state.view.erase({k_begin, k_begin + k_size});
    }

    db_view::iterator& get_it(uint32_t index) {
        auto& state = derived().state;
        if (index >= state.iterators.size() || !state.iterators[index])
            throw std::runtime_error("iterator does not exist");
        return *state.iterators[index];
    }

    void check(const rocksdb::Status& status) {
        if (!status.IsNotFound() && !status.ok())
            throw std::runtime_error("rocksdb error: " + status.ToString());
    }

    uint32_t kv_it_create(const char* prefix, uint32_t size) {
        // todo: reuse destroyed slots?
        auto& state = derived().state;
        derived().check_bounds(prefix, size);
        state.iterators.push_back(std::make_unique<db_view::iterator>(state.view, std::vector<char>{prefix, prefix + size}));
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
        return kv && kv->key.end - kv->key.pos == size && !memcmp(kv->key.pos, key, size);
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
        auto actual_size = kv->key.end - kv->key.pos;
        if (actual_size >= 0x8000'0000)
            throw std::runtime_error("kv size is too big for wasm");
        memcpy(dest, kv->key.pos, std::min(size, (uint32_t)actual_size));
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
        auto actual_size = kv->value.end - kv->value.pos;
        if (actual_size >= 0x8000'0000)
            throw std::runtime_error("kv size is too big for wasm");
        memcpy(dest, kv->value.pos, std::min(size, (uint32_t)actual_size));
        return actual_size;
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
