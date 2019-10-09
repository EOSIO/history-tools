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

    database(const char* db_path, const char* ro_path, std::optional<uint32_t> threads, std::optional<uint32_t> max_open_files) {
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

        options.memtable_factory                = std::make_shared<rocksdb::VectorRepFactory>();
        options.allow_concurrent_memtable_write = false;
        if (max_open_files)
            options.max_open_files = *max_open_files;

        if (ro_path) {
            ilog("open secondary ${p} ${s}", ("p", db_path)("s", ro_path));
            check(rocksdb::DB::OpenAsSecondary(options, db_path, ro_path, &p), "rocksdb::DB::OpenAsSecondary: ");
        } else {
            ilog("open primary ${p}", ("p", db_path));
            check(rocksdb::DB::Open(options, db_path, &p), "rocksdb::DB::Open: ");
        }
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

template <typename T>
std::optional<T> get(database& db, const std::vector<char>& key, bool required = true) {
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
void for_each(database& db, const std::vector<char>& lower_bound, const std::vector<char>& upper_bound, F f) {
    std::unique_ptr<rocksdb::Iterator> it{db.db->NewIterator(rocksdb::ReadOptions())};
    for (it->Seek(to_slice(lower_bound)); it->Valid(); it->Next()) {
        auto k = it->key();
        if (memcmp(k.data(), upper_bound.data(), std::min(k.size(), upper_bound.size())) > 0)
            break;
        if (!f(to_input_buffer(k), to_input_buffer(it->value())))
            return;
    }
    check(it->status(), "for_each: ");
}

// Loop through keys in range [lower_bound, upper_bound], inclusive. Skip keys with duplicate prefix.
// The prefix is the same size as lower_bound and upper_bound, which must have the same size.
//
// bool f(const std::vector& prefix, abieos::input_buffer whole_key, abieos::input_buffer data);
// * return true to continue loop
// * return false to break out of loop
template <typename F>
void for_each_subkey(database& db, std::vector<char> lower_bound, const std::vector<char>& upper_bound, F f) {
    if (lower_bound.size() != upper_bound.size())
        throw std::runtime_error("for_each_subkey: key sizes don't match");
    std::unique_ptr<rocksdb::Iterator> it{db.db->NewIterator(rocksdb::ReadOptions())};
    it->Seek(to_slice(lower_bound));
    while (it->Valid()) {
        auto k = it->key();
        if (memcmp(k.data(), upper_bound.data(), std::min(k.size(), upper_bound.size())) > 0)
            break;
        if (k.size() < lower_bound.size())
            throw std::runtime_error("for_each_subkey: found key with size < prefix");
        memmove(lower_bound.data(), k.data(), lower_bound.size());
        if (!f(std::as_const(lower_bound), to_input_buffer(k), to_input_buffer(it->value())))
            return;
        kv::inc_key(lower_bound);
        it->Seek(to_slice(lower_bound));
    }
    check(it->status(), "for_each_subkey: ");
}

} // namespace rdb
} // namespace state_history
