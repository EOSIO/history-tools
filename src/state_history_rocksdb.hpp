// copyright defined in LICENSE.txt

#pragma once
#include "state_history_kv.hpp"

#include <boost/filesystem.hpp>
#include <fc/exception/exception.hpp>
#include <rocksdb/db.h>

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

inline rocksdb::Slice to_slice(abieos::input_buffer v) { return {v.pos, size_t(v.end - v.pos)}; }

inline abieos::input_buffer to_input_buffer(rocksdb::Slice v) { return {v.data(), v.data() + v.size()}; }

inline abieos::input_buffer to_input_buffer(rocksdb::PinnableSlice& v) { return {v.data(), v.data() + v.size()}; }

inline void put(rocksdb::WriteBatch& batch, const std::vector<char>& key, const std::vector<char>& value, bool overwrite = false) {
    // !!! remove overwrite
    batch.Put(to_slice(key), to_slice(value));
}

template <typename T>
void put(rocksdb::WriteBatch& batch, const std::vector<char>& key, const T& value, bool overwrite = false) {
    put(batch, key, abieos::native_to_bin(value), overwrite);
}

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

inline std::optional<abieos::input_buffer> get_raw(rocksdb::Iterator& it, const std::vector<char>& key, bool required) {
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

template <typename T>
std::optional<T> get(rocksdb::Iterator& it, const std::vector<char>& key, bool required) {
    auto bin = get_raw(it, key, required);
    if (bin)
        return abieos::bin_to_native<T>(*bin);
    else
        return {};
}

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

class db_view {
  public:
    class iterator;
    using bytes        = std::vector<char>;
    using input_buffer = abieos::input_buffer;

    struct key_value {
        input_buffer key   = {};
        input_buffer value = {};
    };

    struct key_present_value {
        input_buffer key     = {};
        bool         present = {};
        input_buffer value   = {};
    };

    struct present_value {
        bool  present = {};
        bytes value   = {};
    };

  private:
    static int key_compare(input_buffer a, input_buffer b) {
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

    state_history::rdb::database db{"db.rocksdb", {}, {}, true};
    rocksdb::WriteBatch          write_batch;
    change_map                   changes;

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
                return {
                    {state_history::rdb::to_input_buffer(rocks_it->key()), true, state_history::rdb::to_input_buffer(rocks_it->value())}};
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

        bool is_end() const { return !impl || impl->is_end(); }

        std::optional<key_value> get_kv() const {
            if (impl)
                return impl->get_kv();
            else
                return {};
        }
    };

    void discard_changes() {
        write_batch.Clear();
        changes.clear();
    }

    void write_changes() {
        write(db, write_batch);
        db.flush(true, true);
        discard_changes();
    }

    void set(input_buffer k, input_buffer v) {
        write_batch.Put(state_history::rdb::to_slice(k), state_history::rdb::to_slice(v));
        changes[{k.pos, k.end}] = {true, {v.pos, v.end}};
    }

    void erase(input_buffer k) {
        write_batch.Delete(state_history::rdb::to_slice(k));
        changes[{k.pos, k.end}] = {false, {}};
    }
}; // db_view

} // namespace rdb
} // namespace state_history
