// copyright defined in LICENSE.txt

#pragma once
#include "state_history_kv.hpp"

#include <boost/filesystem.hpp>
#include <lmdb.h>

namespace state_history {
namespace lmdb {

template <typename T>
inline T* addr(T&& x) {
    return &x;
}

inline void check(int stat, const char* prefix) {
    if (!stat)
        return;
    if (stat == MDB_MAP_FULL)
        throw std::runtime_error(std::string(prefix) + "MDB_MAP_FULL: database hit size limit. Use --lmdb-set-db-size-gb to increase.");
    else
        throw std::runtime_error(std::string(prefix) + mdb_strerror(stat));
}

struct env {
    MDB_env* e = nullptr;

    env(const boost::filesystem::path& db_path, uint32_t db_size_gb = 0) {
        check(mdb_env_create(&e), "mdb_env_create: ");
        if (db_size_gb)
            check(mdb_env_set_mapsize(e, size_t(db_size_gb) * 1024 * 1024 * 1024), "mdb_env_set_mapsize");
        boost::filesystem::create_directories(db_path);
        auto stat = mdb_env_open(e, db_path.c_str(), MDB_NOTLS, 0600);
        if (stat) {
            mdb_env_close(e);
            check(stat, "mdb_env_open: ");
        }
    }

    env(const env&) = delete;
    env(env&&)      = delete;
    env& operator=(const env&) = delete;
    env& operator=(env&&) = delete;

    ~env() { mdb_env_close(e); }
};

struct transaction {
    struct env* env = nullptr;
    MDB_txn*    tx  = nullptr;

    transaction(struct env& env, bool enable_write) {
        check(mdb_txn_begin(env.e, nullptr, enable_write ? 0 : MDB_RDONLY, &tx), "mdb_txn_begin: ");
    }

    transaction(const transaction&) = delete;
    transaction(transaction&&)      = delete;
    transaction& operator=(const transaction&) = delete;
    transaction& operator=(transaction&&) = delete;

    ~transaction() {
        if (tx)
            mdb_txn_abort(tx);
    }

    void commit() {
        auto stat = mdb_txn_commit(tx);
        tx        = nullptr;
        check(stat, "mdb_txn_commit: ");
    }
};

struct database {
    MDB_dbi db;

    database(env& env) {
        transaction t{env, true};
        check(mdb_dbi_open(t.tx, nullptr, 0, &db), "mdb_dbi_open: ");
    }

    database(const database&) = delete;
    database(database&&)      = delete;
    database& operator=(const database&) = delete;
    database& operator=(database&&) = delete;
};

struct cursor {
    MDB_cursor* c = nullptr;

    cursor(transaction& t, database& d) { check(mdb_cursor_open(t.tx, d.db, &c), "mdb_cursor_open: "); }
    cursor(const cursor&) = delete;
    cursor(cursor&&)      = delete;
    cursor& operator=(const cursor&) = delete;
    cursor& operator=(cursor&&) = delete;

    ~cursor() { mdb_cursor_close(c); }
};

inline MDB_val to_val(std::vector<char>& v) { return {v.size(), v.data()}; }

inline MDB_val to_const_val(const std::vector<char>& v) { return {v.size(), const_cast<char*>(v.data())}; }

inline MDB_val to_const_val(abieos::input_buffer v) { return {size_t(v.end - v.pos), const_cast<char*>(v.pos)}; }

inline abieos::input_buffer to_input_buffer(MDB_val v) { return {(char*)v.mv_data, (char*)v.mv_data + v.mv_size}; }

inline bool exists(transaction& t, database& d, MDB_val key) {
    MDB_val v;
    auto    stat = mdb_get(t.tx, d.db, &key, &v);
    if (stat == MDB_NOTFOUND)
        return false;
    check(stat, "mdb_get: ");
    return true;
}

inline abieos::input_buffer get_raw(transaction& t, database& d, MDB_val key, bool required = true) {
    MDB_val v;
    auto    stat = mdb_get(t.tx, d.db, &key, &v);
    if (stat == MDB_NOTFOUND && !required)
        return {};
    check(stat, "mdb_get: ");
    return to_input_buffer(v);
}

inline abieos::input_buffer get_raw(transaction& t, database& d, const std::vector<char>& key, bool required = true) {
    return get_raw(t, d, to_const_val(key), required);
}

inline abieos::input_buffer get_raw(transaction& t, database& d, abieos::input_buffer key, bool required = true) {
    return get_raw(t, d, to_const_val(key), required);
}

inline void put(transaction& t, database& d, const std::vector<char>& key, const std::vector<char>& value, bool overwrite = false) {
    auto v = to_const_val(value);
    check(mdb_put(t.tx, d.db, addr(to_const_val(key)), &v, overwrite ? 0 : MDB_NOOVERWRITE), "mdb_put: ");
}

template <typename T>
void put(transaction& t, database& d, const std::vector<char>& key, const T& value, bool overwrite = false) {
    put(t, d, key, abieos::native_to_bin(value), overwrite);
}

template <typename T>
std::optional<T> get(transaction& t, database& d, const std::vector<char>& key, bool required = true) {
    auto bin = get_raw(t, d, key, required);
    if (!bin.pos)
        return {};
    return abieos::bin_to_native<T>(bin);
}

// Loop through keys in range [lower_bound, upper_bound], inclusive. lower_bound and upper_bound may
// be partial keys (prefixes). They may be different sizes. Does not skip keys with duplicate prefixes.
//
// bool f(abieos::input_buffer key, abieos::input_buffer data);
// * return true to continue loop
// * return false to break out of loop
template <typename F>
void for_each(transaction& t, database& d, const std::vector<char>& lower_bound, const std::vector<char>& upper_bound, F f) {
    cursor  c{t, d};
    auto    k = to_const_val(lower_bound);
    MDB_val v;
    auto    stat = mdb_cursor_get(c.c, &k, &v, MDB_SET_RANGE);
    while (!stat && memcmp(k.mv_data, upper_bound.data(), std::min(k.mv_size, upper_bound.size())) <= 0) {
        if (!f(to_input_buffer(k), to_input_buffer(v)))
            return;
        stat = mdb_cursor_get(c.c, &k, &v, MDB_NEXT_NODUP);
    }
    if (stat != MDB_NOTFOUND)
        check(stat, "for_each: ");
}

// Loop through keys in range [lower_bound, upper_bound], inclusive. Skip keys with duplicate prefix.
// The prefix is the same size as lower_bound and upper_bound, which must have the same size.
//
// bool f(const std::vector& prefix, abieos::input_buffer whole_key, abieos::input_buffer data);
// * return true to continue loop
// * return false to break out of loop
template <typename F>
void for_each_subkey(transaction& t, database& d, const std::vector<char>& lower_bound, const std::vector<char>& upper_bound, F f) {
    if (lower_bound.size() != upper_bound.size())
        throw std::runtime_error("for_each_subkey: key sizes don't match");
    std::vector<char> prefix = lower_bound;
    cursor            c{t, d};
    MDB_val           k = to_const_val(prefix);
    MDB_val           v;
    auto              stat = mdb_cursor_get(c.c, &k, &v, MDB_SET_RANGE);
    while (!stat && memcmp(k.mv_data, upper_bound.data(), std::min(k.mv_size, upper_bound.size())) <= 0) {
        if (k.mv_size < prefix.size())
            throw std::runtime_error("for_each_subkey: found key with size < prefix");
        memmove(prefix.data(), k.mv_data, prefix.size());
        if (!f(const_cast<const std::vector<char>&>(prefix), to_input_buffer(k), to_input_buffer(v)))
            return;
        kv::inc_key(prefix);
        k    = to_const_val(prefix);
        stat = mdb_cursor_get(c.c, &k, &v, MDB_SET_RANGE);
    }
    if (stat != MDB_NOTFOUND)
        check(stat, "for_each_subkey: ");
}

} // namespace lmdb
} // namespace state_history
