// copyright defined in LICENSE.txt

#pragma once
#include "state_history.hpp"

#include <lmdb.h>

namespace state_history {
namespace lmdb {

using namespace abieos::literals;

// clang-format off
inline const std::map<std::string, abieos::name> table_names = {
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
        std::is_integral_v<T> || std::is_same_v<std::decay_t<T>, abieos::name> || std::is_same_v<std::decay_t<T>, abieos::uint128>)
        reverse_bin(bin, f);
    else
        throw std::runtime_error("unsupported key type");
}

template <typename T>
void native_to_bin_key(std::vector<char>& bin, const T& obj) {
    fixup_key<T>(bin, [&] { abieos::native_to_bin(bin, obj); });
}

struct lmdb_type {
    void (*bin_to_bin)(std::vector<char>&, abieos::input_buffer&)     = nullptr;
    void (*bin_to_bin_key)(std::vector<char>&, abieos::input_buffer&) = nullptr;
};

template <typename T>
void bin_to_bin(std::vector<char>& dest, abieos::input_buffer& bin) {
    abieos::native_to_bin(dest, abieos::bin_to_native<T>(bin));
}

template <>
void bin_to_bin<abieos::uint128>(std::vector<char>& dest, abieos::input_buffer& bin) {
    bin_to_bin<uint64_t>(dest, bin);
    bin_to_bin<uint64_t>(dest, bin);
}

template <>
void bin_to_bin<abieos::int128>(std::vector<char>& dest, abieos::input_buffer& bin) {
    bin_to_bin<uint64_t>(dest, bin);
    bin_to_bin<uint64_t>(dest, bin);
}

template <>
void bin_to_bin<state_history::transaction_status>(std::vector<char>& dest, abieos::input_buffer& bin) {
    return bin_to_bin<std::underlying_type_t<state_history::transaction_status>>(dest, bin);
}

template <typename T>
void bin_to_bin_key(std::vector<char>& dest, abieos::input_buffer& bin) {
    fixup_key<T>(dest, [&] { bin_to_bin<T>(dest, bin); });
}

template <typename T>
constexpr lmdb_type make_lmdb_type_for() {
    return lmdb_type{bin_to_bin<T>, bin_to_bin_key<T>};
}

// clang-format off
const std::map<std::string, lmdb_type> abi_type_to_lmdb_type = {
    {"bool",                    make_lmdb_type_for<bool>()},
    {"varuint32",               make_lmdb_type_for<abieos::varuint32>()},
    {"uint8",                   make_lmdb_type_for<uint8_t>()},
    {"uint16",                  make_lmdb_type_for<uint16_t>()},
    {"uint32",                  make_lmdb_type_for<uint32_t>()},
    {"uint64",                  make_lmdb_type_for<uint64_t>()},
    {"uint128",                 make_lmdb_type_for<abieos::uint128>()},
    {"int8",                    make_lmdb_type_for<int8_t>()},
    {"int16",                   make_lmdb_type_for<int16_t>()},
    {"int32",                   make_lmdb_type_for<int32_t>()},
    {"int64",                   make_lmdb_type_for<int64_t>()},
    {"int128",                  make_lmdb_type_for<abieos::int128>()},
    {"float64",                 make_lmdb_type_for<double>()},
    {"float128",                make_lmdb_type_for<abieos::float128>()},
    {"name",                    make_lmdb_type_for<abieos::name>()},
    {"string",                  make_lmdb_type_for<std::string>()},
    {"time_point",              make_lmdb_type_for<abieos::time_point>()},
    {"time_point_sec",          make_lmdb_type_for<abieos::time_point_sec>()},
    {"block_timestamp_type",    make_lmdb_type_for<abieos::block_timestamp>()},
    {"checksum256",             make_lmdb_type_for<abieos::checksum256>()},
    {"public_key",              make_lmdb_type_for<abieos::public_key>()},
    {"bytes",                   make_lmdb_type_for<abieos::bytes>()},
    {"transaction_status",      make_lmdb_type_for<state_history::transaction_status>()},
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
        throw std::runtime_error(std::string(prefix) + "MDB_MAP_FULL: database hit size limit. Use --flm-set-db-size-mb to increase.");
    else
        throw std::runtime_error(std::string(prefix) + mdb_strerror(stat));
}

struct env {
    MDB_env* e = nullptr;

    env(uint32_t db_size_mb = 0) {
        check(mdb_env_create(&e), "mdb_env_create: ");
        if (db_size_mb)
            check(mdb_env_set_mapsize(e, size_t(db_size_mb) * 1024 * 1024), "mdb_env_set_mapsize");
        auto stat = mdb_env_open(e, "foo", 0, 0600);
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
    env*     env = nullptr;
    MDB_txn* tx  = nullptr;

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

inline abieos::input_buffer get_raw(transaction& t, database& d, const std::vector<char>& key, bool required = true) {
    MDB_val v;
    auto    stat = mdb_get(t.tx, d.db, addr(to_const_val(key)), &v);
    if (stat == MDB_NOTFOUND && !required)
        return {};
    check(stat, "mdb_get: ");
    return to_input_buffer(v);
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
T get(transaction& t, database& d, const std::vector<char>& key, bool required = true) {
    auto bin = get_raw(t, d, key, required);
    if (!bin.pos)
        return {};
    return abieos::bin_to_native<T>(bin);
}

template <typename F>
void for_each(transaction& t, database& d, const std::vector<char>& lower, const std::vector<char>& upper, F f) {
    cursor  c{t, d};
    auto    k = to_const_val(lower);
    MDB_val v;
    auto    stat = mdb_cursor_get(c.c, &k, &v, MDB_SET_RANGE);
    while (!stat && memcmp(k.mv_data, upper.data(), std::min(k.mv_size, upper.size())) <= 0) {
        f(to_input_buffer(k), to_input_buffer(v));
        stat = mdb_cursor_get(c.c, &k, &v, MDB_NEXT_NODUP);
    }
    if (stat != MDB_NOTFOUND)
        check(stat, "for_each: ");
}

enum class key_tag : uint8_t {
    fill_status,
    block,
    received_block,
    block_info,
    delta,
    table_index,
};

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

struct fill_status {
    uint32_t            head            = {};
    abieos::checksum256 head_id         = {};
    uint32_t            irreversible    = {};
    abieos::checksum256 irreversible_id = {};
    uint32_t            first           = {};
};

template <typename F>
constexpr void for_each_field(fill_status*, F f) {
    f("head", abieos::member_ptr<&fill_status::head>{});
    f("head_id", abieos::member_ptr<&fill_status::head_id>{});
    f("irreversible", abieos::member_ptr<&fill_status::irreversible>{});
    f("irreversible_id", abieos::member_ptr<&fill_status::irreversible_id>{});
    f("first", abieos::member_ptr<&fill_status::first>{});
}

inline std::vector<char> make_fill_status_key() {
    std::vector<char> result;
    native_to_bin_key(result, (uint8_t)key_tag::fill_status);
    return result;
}

struct received_block {
    uint32_t            block_index = {};
    abieos::checksum256 block_id    = {};
};

template <typename F>
constexpr void for_each_field(received_block*, F f) {
    f("block_index", abieos::member_ptr<&received_block::block_index>{});
    f("block_id", abieos::member_ptr<&received_block::block_id>{});
}

inline std::vector<char> make_received_block_key(uint32_t block) {
    std::vector<char> result;
    native_to_bin_key(result, (uint8_t)key_tag::block);
    native_to_bin_key(result, block);
    native_to_bin_key(result, (uint8_t)key_tag::received_block);
    return result;
}

struct block_info {
    uint32_t                         block_index       = {};
    abieos::checksum256              block_id          = {};
    abieos::block_timestamp          timestamp         = {};
    abieos::name                     producer          = {};
    uint16_t                         confirmed         = {};
    abieos::checksum256              previous          = {};
    abieos::checksum256              transaction_mroot = {};
    abieos::checksum256              action_mroot      = {};
    uint32_t                         schedule_version  = {};
    state_history::producer_schedule new_producers     = {};
};

template <typename F>
constexpr void for_each_field(block_info*, F f) {
    f("block_index", abieos::member_ptr<&block_info::block_index>{});
    f("block_id", abieos::member_ptr<&block_info::block_id>{});
    f("timestamp", abieos::member_ptr<&block_info::timestamp>{});
    f("producer", abieos::member_ptr<&block_info::producer>{});
    f("confirmed", abieos::member_ptr<&block_info::confirmed>{});
    f("previous", abieos::member_ptr<&block_info::previous>{});
    f("transaction_mroot", abieos::member_ptr<&block_info::transaction_mroot>{});
    f("action_mroot", abieos::member_ptr<&block_info::action_mroot>{});
    f("schedule_version", abieos::member_ptr<&block_info::schedule_version>{});
    f("new_producers", abieos::member_ptr<&block_info::new_producers>{});
}

inline std::vector<char> make_block_info_key(uint32_t block_index) {
    std::vector<char> result;
    native_to_bin_key(result, (uint8_t)key_tag::block);
    native_to_bin_key(result, block_index);
    native_to_bin_key(result, (uint8_t)key_tag::block_info);
    return result;
}

inline std::vector<char> make_delta_key(uint32_t block, bool present, abieos::name table) {
    std::vector<char> result;
    native_to_bin_key(result, (uint8_t)key_tag::delta);
    native_to_bin_key(result, table);
    native_to_bin_key(result, block);
    native_to_bin_key(result, present);
    return result;
}

inline std::vector<char> make_table_index_key_prefix(abieos::name table, abieos::name index) {
    std::vector<char> result;
    native_to_bin_key(result, (uint8_t)key_tag::table_index);
    native_to_bin_key(result, table);
    native_to_bin_key(result, index);
    return result;
}

inline void append_table_index_key_suffix(std::vector<char>& dest, uint32_t block, bool present) {
    native_to_bin_key(dest, ~block);
    native_to_bin_key(dest, !present);
}

} // namespace lmdb
} // namespace state_history
