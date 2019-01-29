// copyright defined in LICENSE.txt

#include "state_history.hpp"

#include <lmdb.h>

namespace state_history {
namespace lmdb {

inline void check(int stat, const char* prefix) {
    if (!stat)
        return;
    if (stat == MDB_MAP_FULL)
        throw std::runtime_error(std::string(prefix) + "MDB_MAP_FULL: database hit size limit. Use --set-db-size-mb to increase.");
    else
        throw std::runtime_error(std::string(prefix) + mdb_strerror(stat));
}

struct env {
    MDB_env* e = nullptr;

    env(uint32_t db_size_mb = 0) {
        check(mdb_env_create(&e), "mdb_env_create: ");
        if (db_size_mb)
            check(mdb_env_set_mapsize(e, db_size_mb * 1024 * 1024), "mdb_env_set_mapsize");
        auto stat = mdb_env_open(e, "foo", 0, 0600);
        if (stat) {
            mdb_env_close(e);
            check(stat, "mdb_env_open: ");
        }
    }

    ~env() { mdb_env_close(e); }
};

struct transaction {
    env*     env = nullptr;
    MDB_txn* tx  = nullptr;

    transaction(struct env& env, bool enable_write) {
        check(mdb_txn_begin(env.e, nullptr, enable_write ? 0 : MDB_RDONLY, &tx), "mdb_txn_begin: ");
    }

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
};

inline abieos::input_buffer get_raw(transaction& t, database& d, const std::vector<char>& key, bool required = true) {
    MDB_val k{key.size(), const_cast<char*>(key.data())};
    MDB_val v;
    auto    stat = mdb_get(t.tx, d.db, &k, &v);
    if (stat == MDB_NOTFOUND && !required)
        return {};
    check(stat, "mdb_get: ");
    return {(char*)v.mv_data, (char*)v.mv_data + v.mv_size};
}

inline void put(transaction& t, database& d, const std::vector<char>& key, const std::vector<char>& value, bool overwrite = false) {
    MDB_val k{key.size(), const_cast<char*>(key.data())};
    MDB_val v{value.size(), const_cast<char*>(value.data())};
    check(mdb_put(t.tx, d.db, &k, &v, overwrite ? 0 : MDB_NOOVERWRITE), "mdb_put: ");
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

enum class key_tag : uint8_t {
    fill_status,
    block,
    received_block,
    block_info,
    delta,
    table_index,
};

template <typename T>
void reversed_native_to_bin(std::vector<char>& bin, const T& obj) {
    auto s = bin.size();
    abieos::native_to_bin(bin, obj);
    std::reverse(bin.begin() + s, bin.end());
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
    reversed_native_to_bin(result, (uint8_t)key_tag::fill_status);
    return result;
}

struct received_block {
    abieos::checksum256 block_id = {};
};

template <typename F>
constexpr void for_each_field(received_block*, F f) {
    f("block_id", abieos::member_ptr<&received_block::block_id>{});
}

inline std::vector<char> make_received_block_key(uint32_t block) {
    std::vector<char> result;
    reversed_native_to_bin(result, (uint8_t)key_tag::block);
    reversed_native_to_bin(result, block);
    reversed_native_to_bin(result, (uint8_t)key_tag::received_block);
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
    reversed_native_to_bin(result, (uint8_t)key_tag::block);
    reversed_native_to_bin(result, block_index);
    reversed_native_to_bin(result, (uint8_t)key_tag::block_info);
    return result;
}

inline std::vector<char> make_delta_key(uint32_t block, bool present, abieos::name table) {
    std::vector<char> result;
    reversed_native_to_bin(result, (uint8_t)key_tag::delta);
    reversed_native_to_bin(result, table);
    reversed_native_to_bin(result, block);
    reversed_native_to_bin(result, present);
    // !!! pk fields
    return result;
}

inline std::vector<char> make_table_index_key(uint32_t block, bool present, abieos::name table, abieos::name index) {
    std::vector<char> result;
    reversed_native_to_bin(result, (uint8_t)key_tag::table_index);
    reversed_native_to_bin(result, table);
    reversed_native_to_bin(result, index);
    // !!! index fields
    // !!! pk fields
    reversed_native_to_bin(result, ~block);
    reversed_native_to_bin(result, !present);
    return result;
}

} // namespace lmdb
} // namespace state_history
