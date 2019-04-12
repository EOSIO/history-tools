// copyright defined in LICENSE.txt

#pragma once
#include "query_config.hpp"
#include "state_history.hpp"

#include <boost/filesystem.hpp>
#include <lmdb.h>

namespace state_history {
namespace lmdb {

using namespace abieos::literals;

// clang-format off
inline const std::map<std::string, abieos::name> table_names = {
    {"block_info",                  "block.info"_n},
    {"transaction_trace",           "ttrace"_n},
    {"action_trace",                "atrace"_n},

    {"account",                     "account"_n},
    {"contract_table",              "c.table"_n},
    {"contract_row",                "c.row"_n},
    {"contract_index64",            "c.index64"_n},
    {"contract_index128",           "c.index128"_n},
    {"contract_index256",           "c.index128"_n},
    {"contract_index_double",       "c.index.d"_n},
    {"contract_index_long_double",  "c.index.ld"_n},
    {"global_property",             "glob.prop"_n},
    {"generated_transaction",       "gen.tx"_n},
    {"permission",                  "permission"_n},
    {"permission_link",             "perm.link"_n},
    {"resource_limits",             "res.lim"_n},
    {"resource_usage",              "res.usage"_n},
    {"resource_limits_state",       "res.lim.stat"_n},
    {"resource_limits_config",      "res.lim.conf"_n},
};
// clang-format on

inline void inc_key(std::vector<char>& key) {
    for (auto it = key.rbegin(); it != key.rend(); ++it)
        if (++*it)
            return;
}

template <typename F>
void reverse_bin(std::vector<char>& bin, F f) {
    auto s = bin.size();
    f();
    std::reverse(bin.begin() + s, bin.end());
}

// Modify serialization of types so lexigraphical sort matches data sort
template <typename T, typename F>
void fixup_key(std::vector<char>& bin, F f) {
    if constexpr (
        std::is_unsigned_v<T> || std::is_same_v<std::decay_t<T>, abieos::name> || std::is_same_v<std::decay_t<T>, abieos::uint128> ||
        std::is_same_v<std::decay_t<T>, abieos::checksum256>)
        reverse_bin(bin, f);
    else
        throw std::runtime_error("unsupported key type");
}

template <typename T>
void native_to_bin_key(std::vector<char>& bin, const T& obj) {
    fixup_key<T>(bin, [&] { abieos::native_to_bin(bin, obj); });
}

template <typename T>
T bin_to_native_key(abieos::input_buffer& b) {
    if constexpr (
        std::is_unsigned_v<T> || std::is_same_v<std::decay_t<T>, abieos::name> || std::is_same_v<std::decay_t<T>, abieos::uint128> ||
        std::is_same_v<std::decay_t<T>, abieos::checksum256>) {
        if (b.pos + sizeof(T) > b.end)
            throw std::runtime_error("key deserialization error");
        std::vector<char> v(b.pos, b.pos + sizeof(T));
        b.pos += sizeof(T);
        std::reverse(v.begin(), v.end());
        auto br = abieos::input_buffer{v.data(), v.data() + v.size()};
        return abieos::bin_to_native<T>(br);
    } else {
        throw std::runtime_error("unsupported key type");
    }
}

struct type {
    void (*bin_to_bin)(std::vector<char>&, abieos::input_buffer&)     = nullptr;
    void (*bin_to_bin_key)(std::vector<char>&, abieos::input_buffer&) = nullptr;
    void (*lower_bound_key)(std::vector<char>&)                       = nullptr;
    void (*upper_bound_key)(std::vector<char>&)                       = nullptr;
    uint32_t (*get_fixed_size)()                                      = nullptr;
};

template <typename T>
void bin_to_bin(std::vector<char>& dest, abieos::input_buffer& bin) {
    abieos::native_to_bin(dest, abieos::bin_to_native<T>(bin));
}

template <>
inline void bin_to_bin<abieos::uint128>(std::vector<char>& dest, abieos::input_buffer& bin) {
    bin_to_bin<uint64_t>(dest, bin);
    bin_to_bin<uint64_t>(dest, bin);
}

template <>
inline void bin_to_bin<abieos::int128>(std::vector<char>& dest, abieos::input_buffer& bin) {
    bin_to_bin<uint64_t>(dest, bin);
    bin_to_bin<uint64_t>(dest, bin);
}

template <>
inline void bin_to_bin<state_history::transaction_status>(std::vector<char>& dest, abieos::input_buffer& bin) {
    return bin_to_bin<std::underlying_type_t<state_history::transaction_status>>(dest, bin);
}

template <typename T>
void bin_to_bin_key(std::vector<char>& dest, abieos::input_buffer& bin) {
    if constexpr (std::is_same_v<std::decay_t<T>, abieos::varuint32>) {
        reverse_bin(dest, [&] { abieos::native_to_bin(dest, abieos::bin_to_native<abieos::varuint32>(bin).value); });
    } else {
        fixup_key<T>(dest, [&] { bin_to_bin<T>(dest, bin); });
    }
}

template <typename T>
void lower_bound_key(std::vector<char>& dest) {
    if constexpr (
        std::is_unsigned_v<T> || std::is_same_v<std::decay_t<T>, abieos::name> || std::is_same_v<std::decay_t<T>, abieos::uint128> ||
        std::is_same_v<std::decay_t<T>, abieos::checksum256>)
        dest.resize(dest.size() + sizeof(T));
    else
        throw std::runtime_error("unsupported key type");
}

template <typename T>
void upper_bound_key(std::vector<char>& dest) {
    if constexpr (
        std::is_unsigned_v<T> || std::is_same_v<std::decay_t<T>, abieos::name> || std::is_same_v<std::decay_t<T>, abieos::uint128> ||
        std::is_same_v<std::decay_t<T>, abieos::checksum256>)
        dest.resize(dest.size() + sizeof(T), 0xff);
    else
        throw std::runtime_error("unsupported key type");
}

template <typename T>
uint32_t get_fixed_size() {
    if constexpr (
        std::is_integral_v<T> || std::is_same_v<std::decay_t<T>, abieos::name> || std::is_same_v<std::decay_t<T>, abieos::uint128> ||
        std::is_same_v<std::decay_t<T>, abieos::checksum256>)
        return sizeof(T);
    else
        return 0;
}

template <typename T>
constexpr type make_type_for() {
    return type{bin_to_bin<T>, bin_to_bin_key<T>, lower_bound_key<T>, upper_bound_key<T>, get_fixed_size<T>};
}

// clang-format off
const inline std::map<std::string, type> abi_type_to_lmdb_type = {
    {"bool",                    make_type_for<bool>()},
    {"varuint32",               make_type_for<abieos::varuint32>()},
    {"uint8",                   make_type_for<uint8_t>()},
    {"uint16",                  make_type_for<uint16_t>()},
    {"uint32",                  make_type_for<uint32_t>()},
    {"uint64",                  make_type_for<uint64_t>()},
    {"uint128",                 make_type_for<abieos::uint128>()},
    {"int8",                    make_type_for<int8_t>()},
    {"int16",                   make_type_for<int16_t>()},
    {"int32",                   make_type_for<int32_t>()},
    {"int64",                   make_type_for<int64_t>()},
    {"int128",                  make_type_for<abieos::int128>()},
    {"float64",                 make_type_for<double>()},
    {"float128",                make_type_for<abieos::float128>()},
    {"name",                    make_type_for<abieos::name>()},
    {"string",                  make_type_for<std::string>()},
    {"time_point",              make_type_for<abieos::time_point>()},
    {"time_point_sec",          make_type_for<abieos::time_point_sec>()},
    {"block_timestamp_type",    make_type_for<abieos::block_timestamp>()},
    {"checksum256",             make_type_for<abieos::checksum256>()},
    {"public_key",              make_type_for<abieos::public_key>()},
    {"bytes",                   make_type_for<abieos::bytes>()},
    {"transaction_status",      make_type_for<state_history::transaction_status>()},
};
// clang-format on

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
        inc_key(prefix);
        k    = to_const_val(prefix);
        stat = mdb_cursor_get(c.c, &k, &v, MDB_SET_RANGE);
    }
    if (stat != MDB_NOTFOUND)
        check(stat, "for_each_subkey: ");
}

// Description                      Notes   Data format         Key format
// =======================================================================================================
// fill_status                              fill_status         key_tag::fill_status
// received_block                   1       received_block      key_tag::block,             block_num,      key_tag::received_block
// table row (non-state tables)     1       row content         key_tag::block,             block_num,      key_tag::table_row,         table_name,         primary key fields
// table delta (state tables)       1       row content         key_tag::block,             block_num,      key_tag::table_delta,       table_name,         present,        primary key fields
// table index (non-state tables)           table delta's key   key_tag::table_index,       table_name,     index_name,                 index fields
// table index (state tables)               table delta's key   key_tag::table_index,       table_name,     index_name,                 index fields,       ~block_num,     !present
// table index reference            2       table index's key   key_tag::table_index_ref,   block_num,      table's key,                table index's key
//
// Notes
//  *: Keys are serialized in lexigraphical sort order. See native_to_bin_key() and bin_to_native_key().
//  1: Erase range lower_bound(make_block_key(n)) to upper_bound(make_block_key()) to erase blocks >= n
//  2: Aids removing index entries

enum class key_tag : uint8_t {
    fill_status     = 0x10,
    block           = 0x20,
    received_block  = 0x30,
    table_row       = 0x50,
    table_delta     = 0x60,
    table_index     = 0x70,
    table_index_ref = 0x80,
};

inline key_tag bin_to_key_tag(abieos::input_buffer& b) { return (key_tag)abieos::bin_to_native<uint8_t>(b); }

inline const char* to_string(key_tag t) {
    switch (t) {
    case key_tag::fill_status: return "fill_status";
    case key_tag::block: return "block";
    case key_tag::received_block: return "received_block";
    case key_tag::table_row: return "table_row";
    case key_tag::table_delta: return "table_delta";
    case key_tag::table_index: return "table_index";
    case key_tag::table_index_ref: return "table_index_ref";
    default: return "?";
    }
}

inline std::string key_to_string(abieos::input_buffer b) {
    using std::to_string;
    std::string result;
    auto        t0 = bin_to_key_tag(b);
    result += to_string(t0);
    if (t0 == key_tag::block) {
        result += " " + to_string(bin_to_native_key<uint32_t>(b));
        auto t1 = bin_to_key_tag(b);
        result += " " + std::string{to_string(t1)};
        if (t1 == key_tag::table_row) {
            auto table_name = bin_to_native_key<abieos::name>(b);
            result += " '" + (std::string)table_name + "' ";
            abieos::hex(b.pos, b.end, std::back_inserter(result));
        } else if (t1 == key_tag::table_delta) {
            auto table_name = bin_to_native_key<abieos::name>(b);
            result += " '" + (std::string)table_name + "' present: " + (bin_to_native_key<bool>(b) ? "true" : "false") + " ";
            abieos::hex(b.pos, b.end, std::back_inserter(result));
        } else {
            result += " ...";
        }
    } else {
        result += " ...";
    }
    return result;
} // namespace lmdb

inline std::vector<char> make_block_key() {
    std::vector<char> result;
    native_to_bin_key(result, (uint8_t)key_tag::block);
    return result;
}

inline std::vector<char> make_block_key(uint32_t block) {
    std::vector<char> result;
    native_to_bin_key(result, (uint8_t)key_tag::block);
    native_to_bin_key(result, block);
    return result;
}

inline std::vector<char> make_fill_status_key() {
    std::vector<char> result;
    native_to_bin_key(result, (uint8_t)key_tag::fill_status);
    return result;
}

struct received_block {
    uint32_t            block_num = {};
    abieos::checksum256 block_id  = {};
};

template <typename F>
constexpr void for_each_field(received_block*, F f) {
    f("block_num", abieos::member_ptr<&received_block::block_num>{});
    f("block_id", abieos::member_ptr<&received_block::block_id>{});
}

inline std::vector<char> make_received_block_key(uint32_t block) {
    std::vector<char> result;
    native_to_bin_key(result, (uint8_t)key_tag::block);
    native_to_bin_key(result, block);
    native_to_bin_key(result, (uint8_t)key_tag::received_block);
    return result;
}

inline std::vector<char> make_table_row_key(uint32_t block) {
    std::vector<char> result;
    native_to_bin_key(result, (uint8_t)key_tag::block);
    native_to_bin_key(result, block);
    native_to_bin_key(result, (uint8_t)key_tag::table_row);
    return result;
}

inline std::vector<char> make_block_info_key(uint32_t block) {
    std::vector<char> result;
    native_to_bin_key(result, (uint8_t)key_tag::block);
    native_to_bin_key(result, block);
    native_to_bin_key(result, (uint8_t)key_tag::table_row);
    native_to_bin_key(result, "block.info"_n);
    return result;
}

inline void append_transaction_trace_key(std::vector<char>& dest, uint32_t block, const abieos::checksum256 transaction_id) {
    native_to_bin_key(dest, (uint8_t)key_tag::block);
    native_to_bin_key(dest, block);
    native_to_bin_key(dest, (uint8_t)key_tag::table_row);
    native_to_bin_key(dest, "ttrace"_n);
    native_to_bin_key(dest, transaction_id);
}

inline void
append_action_trace_key(std::vector<char>& dest, uint32_t block, const abieos::checksum256 transaction_id, uint32_t action_index) {
    native_to_bin_key(dest, (uint8_t)key_tag::block);
    native_to_bin_key(dest, block);
    native_to_bin_key(dest, (uint8_t)key_tag::table_row);
    native_to_bin_key(dest, "atrace"_n);
    native_to_bin_key(dest, transaction_id);
    native_to_bin_key(dest, action_index);
}

inline void append_delta_key(std::vector<char>& dest, uint32_t block) {
    native_to_bin_key(dest, (uint8_t)key_tag::block);
    native_to_bin_key(dest, block);
    native_to_bin_key(dest, (uint8_t)key_tag::table_delta);
}

inline void append_delta_key(std::vector<char>& dest, uint32_t block, bool present, abieos::name table) {
    native_to_bin_key(dest, (uint8_t)key_tag::block);
    native_to_bin_key(dest, block);
    native_to_bin_key(dest, (uint8_t)key_tag::table_delta);
    native_to_bin_key(dest, table);
    native_to_bin_key(dest, present);
}

inline void append_table_index_key(std::vector<char>& dest) { native_to_bin_key(dest, (uint8_t)key_tag::table_index); }

inline void append_table_index_key(std::vector<char>& dest, abieos::name table, abieos::name index) {
    native_to_bin_key(dest, (uint8_t)key_tag::table_index);
    native_to_bin_key(dest, table);
    native_to_bin_key(dest, index);
}

inline std::vector<char> make_table_index_key() {
    std::vector<char> result;
    append_table_index_key(result);
    return result;
}

inline std::vector<char> make_table_index_key(abieos::name table, abieos::name index) {
    std::vector<char> result;
    append_table_index_key(result, table, index);
    return result;
}

inline void append_table_index_state_suffix(std::vector<char>& dest, uint32_t block) { native_to_bin_key(dest, ~block); }

inline void append_table_index_state_suffix(std::vector<char>& dest, uint32_t block, bool present) {
    native_to_bin_key(dest, ~block);
    native_to_bin_key(dest, !present);
}

inline std::vector<char> make_table_index_ref_key() {
    std::vector<char> result;
    native_to_bin_key(result, (uint8_t)key_tag::table_index_ref);
    return result;
}

inline std::vector<char> make_table_index_ref_key(uint32_t block) {
    std::vector<char> result;
    native_to_bin_key(result, (uint8_t)key_tag::table_index_ref);
    native_to_bin_key(result, block);
    return result;
}

inline std::vector<char> make_table_index_ref_key(uint32_t block, const std::vector<char>& table_key) {
    std::vector<char> result;
    native_to_bin_key(result, (uint8_t)key_tag::table_index_ref);
    native_to_bin_key(result, block);
    result.insert(result.end(), table_key.begin(), table_key.end());
    return result;
}

inline std::vector<char>
make_table_index_ref_key(uint32_t block, const std::vector<char>& table_key, const std::vector<char>& table_index_key) {
    std::vector<char> result;
    native_to_bin_key(result, (uint8_t)key_tag::table_index_ref);
    native_to_bin_key(result, block);
    result.insert(result.end(), table_key.begin(), table_key.end());
    result.insert(result.end(), table_index_key.begin(), table_index_key.end());
    return result;
}

struct defs {
    using type = lmdb::type;

    struct field : query_config::field<defs> {
        std::optional<uint32_t> byte_position = {};
    };

    using key = query_config::key<defs>;

    struct table : query_config::table<defs> {
        abieos::name short_name = {};
    };

    using query = query_config::query<defs>;

    struct config : query_config::config<defs> {
        template <typename M>
        void prepare(const M& type_map) {
            query_config::config<defs>::prepare(type_map);
            for (auto& tab : tables) {
                auto it = table_names.find(tab.name);
                if (it == table_names.end())
                    throw std::runtime_error("query_database: unknown table: " + tab.name);
                tab.short_name = it->second;

                uint32_t pos = 0;
                for (auto& field : tab.fields) {
                    field.byte_position = pos;
                    auto size           = field.type_obj->get_fixed_size();
                    if (!size)
                        break;
                    pos += size;
                }
            }
        }
    };
}; // defs

using field  = defs::field;
using key    = defs::key;
using table  = defs::table;
using query  = defs::query;
using config = defs::config;

} // namespace lmdb
} // namespace state_history
