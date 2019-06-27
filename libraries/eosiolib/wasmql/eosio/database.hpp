// copyright defined in LICENSE.txt

// todo: first vs. first_key, last vs. last_key
// todo: results vs. response vs. rows vs. records

#pragma once
#include <eosio/crypto.hpp>
#include <eosio/fixed_bytes.hpp>
#include <eosio/name.hpp>
#include <eosio/shared_memory.hpp>
#include <eosio/struct_reflection.hpp>
#include <eosio/temp_placeholders.hpp>
#include <eosio/time.hpp>
#include <eosio/to_json.hpp>

#include <type_traits>

namespace eosio {

/// \group increment_key Increment Key
/// Increment a database key. Return true if the result wrapped.
inline bool increment_key(uint8_t& key) { return !++key; }

/// \group increment_key
inline bool increment_key(uint16_t& key) { return !++key; }

/// \group increment_key
inline bool increment_key(uint32_t& key) { return !++key; }

/// \group increment_key
inline bool increment_key(uint64_t& key) { return !++key; }

/// \group increment_key
inline bool increment_key(uint128_t& key) { return !++key; }

/// \group increment_key
inline bool increment_key(name& key) { return !++key.value; }

/// \group increment_key
inline bool increment_key(checksum256& key) { return increment_key(key.data()[1]) && increment_key(key.data()[0]); }

/// \output_section Tables
/// Transaction status
enum class transaction_status : uint8_t {
    /// succeed, no error handler executed
    executed = 0,
    /// objectively failed (not executed), error handler executed
    soft_fail = 1,
    /// objectively failed and error handler objectively failed thus no state change
    hard_fail = 2,
    /// transaction delayed/deferred/scheduled for future execution
    delayed = 3,
    /// transaction expired and storage space refunded to user
    expired = 4,
};

/// \group to_json_explicit
__attribute__((noinline)) inline eosio::rope to_json(transaction_status value) { return eosio::int_to_json(std::underlying_type_t<transaction_status>(value)); }

/// Information extracted from a block
struct block_info {
    uint32_t         block_num             = {};
    checksum256      block_id              = {};
    block_timestamp  timestamp             = block_timestamp{};
    name             producer              = {};
    uint16_t         confirmed             = {};
    checksum256      previous              = {};
    checksum256      transaction_mroot     = {};
    checksum256      action_mroot          = {};
    uint32_t         schedule_version      = {};
    uint32_t         new_producers_version = {};
    //std::vector<producer_key> new_producers         = {}; // todo
};

STRUCT_REFLECT(block_info) {
    STRUCT_MEMBER(block_info, block_num)
    STRUCT_MEMBER(block_info, block_id)
    STRUCT_MEMBER(block_info, timestamp)
    STRUCT_MEMBER(block_info, producer)
    STRUCT_MEMBER(block_info, confirmed)
    STRUCT_MEMBER(block_info, previous)
    STRUCT_MEMBER(block_info, transaction_mroot)
    STRUCT_MEMBER(block_info, action_mroot)
    STRUCT_MEMBER(block_info, schedule_version)
    STRUCT_MEMBER(block_info, new_producers_version)
}

/// Information about key activity
struct public_key_history {
    public_key  public_key = {};
    eosio::name name       = {};
    eosio::name permission = {};
};

STRUCT_REFLECT(public_key_history) {
    STRUCT_MEMBER(public_key_history, public_key)
    STRUCT_MEMBER(public_key_history, name)
    STRUCT_MEMBER(public_key_history, permission)
}

/// Details about action execution
struct action_trace {
    uint32_t                               block_index             = {};
    checksum256                            transaction_id          = {};
    uint32_t                               action_index            = {};
    uint32_t                               parent_action_index     = {};
    eosio::transaction_status              transaction_status      = {};
    eosio::name                            receipt_receiver        = {};
    checksum256                            receipt_act_digest      = {};
    uint64_t                               receipt_global_sequence = {};
    uint64_t                               receipt_recv_sequence   = {};
    unsigned_int                           receipt_code_sequence   = {};
    unsigned_int                           receipt_abi_sequence    = {};
    eosio::name                            account                 = {};
    eosio::name                            name                    = {};
    shared_memory<datastream<const char*>> data                    = {};
    bool                                   context_free            = {};
    int64_t                                elapsed                 = {};

    EOSLIB_SERIALIZE(
        action_trace, (block_index)(transaction_id)(action_index)(parent_action_index)(transaction_status)(receipt_receiver)(
                          receipt_act_digest)(receipt_global_sequence)(receipt_recv_sequence)(receipt_code_sequence)(receipt_abi_sequence)(
                          account)(name)(data)(context_free)(elapsed))
};

STRUCT_REFLECT(action_trace) {
    STRUCT_MEMBER(action_trace, block_index)
    STRUCT_MEMBER(action_trace, transaction_id)
    STRUCT_MEMBER(action_trace, action_index)
    STRUCT_MEMBER(action_trace, parent_action_index)
    STRUCT_MEMBER(action_trace, transaction_status)
    STRUCT_MEMBER(action_trace, receipt_receiver)
    STRUCT_MEMBER(action_trace, receipt_act_digest)
    STRUCT_MEMBER(action_trace, receipt_global_sequence)
    STRUCT_MEMBER(action_trace, receipt_recv_sequence)
    STRUCT_MEMBER(action_trace, receipt_code_sequence)
    STRUCT_MEMBER(action_trace, receipt_abi_sequence)
    STRUCT_MEMBER(action_trace, account)
    STRUCT_MEMBER(action_trace, name)
    STRUCT_MEMBER(action_trace, data)
    STRUCT_MEMBER(action_trace, context_free)
    STRUCT_MEMBER(action_trace, elapsed)
}

/// Details about an account
struct account {
    uint32_t                               block_index      = {};
    bool                                   present          = {};
    eosio::name                            name             = {};
    uint8_t                                vm_type          = {};
    uint8_t                                vm_version       = {};
    bool                                   privileged       = {};
    time_point                             last_code_update = time_point{};
    checksum256                            code_version     = {};
    block_timestamp_type                   creation_date    = block_timestamp_type{};
    shared_memory<datastream<const char*>> code             = {};
    shared_memory<datastream<const char*>> abi              = {};

    EOSLIB_SERIALIZE(
        account, (block_index)(present)(name)(vm_type)(vm_version)(privileged)(last_code_update)(code_version)(creation_date)(code)(abi))
};

STRUCT_REFLECT(account) {
    STRUCT_MEMBER(account, block_index)
    STRUCT_MEMBER(account, present)
    STRUCT_MEMBER(account, name)
    STRUCT_MEMBER(account, vm_type)
    STRUCT_MEMBER(account, vm_version)
    STRUCT_MEMBER(account, privileged)
    STRUCT_MEMBER(account, last_code_update)
    STRUCT_MEMBER(account, code_version)
    STRUCT_MEMBER(account, creation_date)
    STRUCT_MEMBER(account, code)
    STRUCT_MEMBER(account, abi)
}

/// A row in a contract's table
struct contract_row {
    uint32_t                               block_index = {};
    bool                                   present     = {};
    name                                   code        = {};
    uint64_t                               scope       = {};
    name                                   table       = {};
    uint64_t                               primary_key = {};
    name                                   payer       = {};
    shared_memory<datastream<const char*>> value       = {};
};

/// \exclude
inline std::string_view schema_type_name(contract_row*) { return "eosio::contract_row"; }

/// A secondary index entry in a contract's table. Also includes fields from `contract_row`.
template <typename T>
struct contract_secondary_index_with_row {
    uint32_t                               block_index     = {};
    bool                                   present         = {};
    name                                   code            = {};
    uint64_t                               scope           = {};
    name                                   table           = {};
    uint64_t                               primary_key     = {};
    name                                   payer           = {};
    T                                      secondary_key   = {};
    uint32_t                               row_block_index = {};
    bool                                   row_present     = {};
    name                                   row_payer       = {};
    shared_memory<datastream<const char*>> row_value       = {};
};

/// \output_section Queries
/// Pass this to `query_database` to get `block_info` for a range of block indexes.
/// The query results are sorted by `block_num`. Every record has a different block_num.
struct query_block_info_range_index {
    /// Identifies query type. Do not modify this field.
    name query_name = "block.info"_n;

    /// Query records with `block_num` in the range [`first`, `last`].
    uint32_t first = {};

    /// Query records with `block_num` in the range [`first`, `last`].
    uint32_t last = {};

    /// Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.
    uint32_t max_results = {};
};

/// Pass this to `query_database` to get `public_key_history` for a public key.
/// The query results are sorted by `name`.
struct query_public_key_history {
    /// Identifies query type. Do not modify this field.
    name query_name = "key.history"_n;

    /// Query records with `public_key` in the range [`first`, `last`].
    public_key first = {};

    /// Query records with `public_key` in the range [`first`, `last`].
    public_key last = {};

    /// Maximum results t return.  The wasm-ql server may cap the number of results to a smaller number.
    uint32_t max_results = {};
};

/// Pass this to `query_database` to get `action_trace` for a range of keys. Only includes actions
/// in executed transactions.
///
/// The query results are sorted by `key`. Every record has a different key.
/// ```c++
/// struct key {
///     eosio::name     name             = {};
///     eosio::name     receipt_receiver = {};
///     eosio::name     account          = {};
///     uint32_t        block_index      = {};
///     checksum256     transaction_id   = {};
///     uint32_t        action_index     = {};
///
///     // Construct the key from `data`
///     static key from_data(const action_trace& data);
/// };
/// ```
struct query_action_trace_executed_range_name_receiver_account_block_trans_action {
    struct key {
        eosio::name name             = {};
        eosio::name receipt_receiver = {};
        eosio::name account          = {};
        uint32_t    block_index      = {};
        checksum256 transaction_id   = {};
        uint32_t    action_index     = {};

        // Extract the key from `data`
        static key from_data(const action_trace& data) {
            return {
                .name             = data.name,
                .receipt_receiver = data.receipt_receiver,
                .account          = data.account,
                .block_index      = data.block_index,
                .transaction_id   = data.transaction_id,
                .action_index     = data.action_index,
            };
        }
    };

    /// Identifies query type. Do not modify this field.
    name query_name = "at.e.nra"_n;

    /// Look at this point of time in history
    uint32_t max_block = {};

    /// Query records with keys in the range [`first`, `last`].
    key first = {};

    /// Query records with keys in the range [`first`, `last`].
    key last = {};

    /// Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.
    uint32_t max_results = {};
};

/// \group increment_key
inline bool increment_key(query_action_trace_executed_range_name_receiver_account_block_trans_action::key& key) {
    return increment_key(key.action_index) &&     //
           increment_key(key.transaction_id) &&   //
           increment_key(key.block_index) &&      //
           increment_key(key.account) &&          //
           increment_key(key.receipt_receiver) && //
           increment_key(key.name);
}


/// Pass this to `query_database` to get `action_trace` for a range of `receipt_receiver` names.
/// The query results are sorted by `key`.  Every record has a unique key.
/// ```c++
/// struct key {
///     eosio::name     receipt_receiver = {};
///     uint32_t        block_index      = {};
///     checksum256     transaction_id   = {};
///     uint32_t        action_index     = {};
///
///     // Construct the key from `data`
///     static key from_data(const action_trace& data);
/// };
/// ```
struct query_action_trace_receipt_receiver {
    struct key {
        eosio::name receipt_receiver = {};
        uint32_t block_index         = {};
        checksum256 transaction_id   = {};
        uint32_t action_index        = {};

        // Extract the key from `data`
        static key from_data(const action_trace& data) {
            return {
                .receipt_receiver = data.receipt_receiver,
                .block_index      = data.block_index,
                .transaction_id   = data.transaction_id,
                .action_index     = data.action_index,
            };
        }
    };

    /// Identifies query type. Do not modify this field.
    name query_name = "receipt.rcvr"_n;

    /// Look at this point of time in history
    uint32_t max_block = {};

    /// Query records with keys in the range [`first`, `last`].
    key first = {};

    /// Query records with keys in the range [`first`, `last`].
    key last = {};

    /// Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.
    uint32_t max_results = {};
};

/// \group increment_key
inline bool increment_key(query_action_trace_receipt_receiver::key& key) {
    return increment_key(key.action_index) &&   //
           increment_key(key.transaction_id) && //
           increment_key(key.block_index) &&    //
           increment_key(key.receipt_receiver);
}


/// Pass this to `query_database` to get a transaction receipt for a transaction id.
/// The query results are sorted by `key`.  Every record has a unique key.
/// ```c++
/// struct key {
///     checksum256 transaction_id = {};
///     uint32_t block_index = {};
///     uint32_t action_index = {};
///
///     // Construct the key from `data`
///     static key from_data(const action_trace& data);
/// };
/// ```
struct query_transaction_receipt {
    struct key {
        checksum256 transaction_id = {};
        uint32_t block_index       = {};
        uint32_t action_index      = {};

        // Extract the key from `data`
        static key from_data(const action_trace& data) {
            return {
                .transaction_id = data.transaction_id,
                .block_index = data.block_index,
                .action_index = data.action_index,
            };
        }
    };

    /// Identifies query type.  Do not modify this field.
    name query_name = "transaction"_n;

    /// Look at this point of time in history
    uint32_t max_block = {};

    /// Query records with keys in the range [`first`, `last`].
    key first = {};

    /// Query records with keys in the range [`first`, `last`].
    key last = {};

    /// Maximum results to return.  The wasm-ql server may cap the number of results to a smaller number.
    uint32_t max_results = {};
};

/// \group increment_key
inline bool increment_key(query_transaction_receipt::key& key) {
    return increment_key(key.action_index) && //
           increment_key(key.block_index) &&  //
           increment_key(key.transaction_id);
}


/// Pass this to `query_database` to get `account` for a range of names.
/// The query results are sorted by `name`. Every record has a different name.
struct query_account_range_name {
    /// Identifies query type. Do not modify this field.
    name query_name = "account"_n;

    /// Look at this point of time in history
    uint32_t max_block = {};

    /// Query records with `name` in the range [`first`, `last`].
    name first = {};

    /// Query records with `name` in the range [`first`, `last`].
    name last = {};

    /// Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.
    uint32_t max_results = {};
};

/// Pass this to `query_database` to get `contract_row` for a range of keys.
///
/// The query results are sorted by `key`. Every record has a different key.
/// ```c++
/// struct key {
///     name     code        = {};
///     name     table       = {};
///     uint64_t primary_key = {};
///     uint64_t scope       = {};
///
///     // Construct the key from `data`
///     static key from_data(const contract_row& data);
/// };
/// ```
struct query_contract_row_range_code_table_pk_scope {
    struct key {
        name     code        = {};
        name     table       = {};
        uint64_t primary_key = {};
        uint64_t scope       = {};

        // Extract the key from `data`
        static key from_data(const contract_row& data) {
            return {
                .code        = data.code,
                .table       = data.table,
                .primary_key = data.primary_key,
                .scope       = data.scope,
            };
        }
    };

    /// Identifies query type. Do not modify this field.
    name query_name = "cr.ctps"_n;

    /// Look at this point of time in history
    uint32_t max_block = {};

    /// Query records with keys in the range [`first`, `last`].
    key first = {};

    /// Query records with keys in the range [`first`, `last`].
    key last = {};

    /// Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.
    uint32_t max_results = {};
};

/// \group increment_key
inline bool increment_key(query_contract_row_range_code_table_pk_scope::key& key) {
    return increment_key(key.scope) &&       //
           increment_key(key.primary_key) && //
           increment_key(key.table) &&       //
           increment_key(key.code);
}

/// Pass this to `query_database` to get `contract_row` for a range of keys.
///
/// The query results are sorted by `key`. Every record has a different key.
/// ```c++
/// struct key {
///     name     code        = {};
///     name     table       = {};
///     uint64_t scope       = {};
///     uint64_t primary_key = {};
///
///     // Construct the key from `data`
///     static key from_data(const contract_row& data);
/// };
/// ```
struct query_contract_row_range_code_table_scope_pk {
    struct key {
        name     code        = {};
        name     table       = {};
        uint64_t scope       = {};
        uint64_t primary_key = {};

        // Extract the key from `data`
        static key from_data(const contract_row& data) {
            return {
                .code        = data.code,
                .table       = data.table,
                .scope       = data.scope,
                .primary_key = data.primary_key,
            };
        }
    };

    /// Identifies query type. Do not modify this field.
    name query_name = "cr.ctsp"_n;

    /// Look at this point of time in history
    uint32_t max_block = {};

    /// Query records with keys in the range [`first`, `last`].
    key first = {};

    /// Query records with keys in the range [`first`, `last`].
    key last = {};

    /// Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.
    uint32_t max_results = {};
};

/// \group increment_key
inline bool increment_key(query_contract_row_range_code_table_scope_pk::key& key) {
    return increment_key(key.primary_key) && //
           increment_key(key.scope) &&       //
           increment_key(key.table) &&       //
           increment_key(key.code);
}

/// Pass this to `query_database` to get `contract_row` for a range of keys.
///
/// The query results are sorted by `key`. Every record has a different key.
/// ```c++
/// struct key {
///     uint64_t scope       = {};
///     name     table       = {};
///     uint64_t primary_key = {};
///     name     code        = {};
///
///     // Construct the key from `data`
///     static key from_data(const contract_row& data);
/// };
/// ```
struct query_contract_row_range_scope_table_pk_code {
    struct key {
        uint64_t scope       = {};
        name     table       = {};
        uint64_t primary_key = {};
        name     code        = {};

        // Extract the key from `data`
        static key from_data(const contract_row& data) {
            return {
                .scope       = data.scope,
                .table       = data.table,
                .primary_key = data.primary_key,
                .code        = data.code,
            };
        }
    };

    /// Identifies query type. Do not modify this field.
    name query_name = "cr.stpc"_n;

    /// Look at this point of time in history
    uint32_t max_block = {};

    /// Query records with keys in the range [`first`, `last`].
    key first = {};

    /// Query records with keys in the range [`first`, `last`].
    key last = {};

    /// Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.
    uint32_t max_results = {};
};

/// \group increment_key
inline bool increment_key(query_contract_row_range_scope_table_pk_code::key& key) {
    return increment_key(key.code) &&        //
           increment_key(key.primary_key) && //
           increment_key(key.table) &&       //
           increment_key(key.scope);
}

/// Pass this to `query_database` to get `contract_secondary_index_with_row<uint64_t>` for a range of keys.
///
/// The query results are sorted by `key`. Every record has a different key.
/// ```c++
/// struct key {
///     name     code          = {};
///     name     table         = {};
///     uint64_t scope         = {};
///     uint64_t secondary_key = {};
///     uint64_t primary_key   = {};
///
///     // Construct the key from `data`
///     static key from_data(const contract_secondary_index_with_row<uint64_t>& data);
/// };
/// ```
struct query_contract_index64_range_code_table_scope_sk_pk {
    struct key {
        name     code          = {};
        name     table         = {};
        uint64_t scope         = {};
        uint64_t secondary_key = {};
        uint64_t primary_key   = {};

        // Extract the key from `data`
        static key from_data(const contract_secondary_index_with_row<uint64_t>& data) {
            return {
                .code          = data.code,
                .table         = data.table,
                .scope         = data.scope,
                .secondary_key = data.secondary_key,
                .primary_key   = data.primary_key,
            };
        }
    };

    /// Identifies query type. Do not modify this field.
    name query_name = "ci1.cts2p"_n;

    /// Look at this point of time in history
    uint32_t max_block = {};

    /// Query records with keys in the range [`first`, `last`].
    key first = {};

    /// Query records with keys in the range [`first`, `last`].
    key last = {};

    /// Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.
    uint32_t max_results = {};
};

/// \group increment_key
inline bool increment_key(query_contract_index64_range_code_table_scope_sk_pk::key& key) {
    return increment_key(key.primary_key) &&   //
           increment_key(key.secondary_key) && //
           increment_key(key.scope) &&         //
           increment_key(key.table) &&         //
           increment_key(key.code);
}

/// \output_section Database Status
/// Status of the database. Returned by `get_database_status`.
struct database_status {
    uint32_t    head            = {};
    checksum256 head_id         = {};
    uint32_t    irreversible    = {};
    checksum256 irreversible_id = {};
    uint32_t    first           = {};
};

/// \exclude
STRUCT_REFLECT(database_status) {
    STRUCT_MEMBER(database_status, head)
    STRUCT_MEMBER(database_status, head_id)
    STRUCT_MEMBER(database_status, irreversible)
    STRUCT_MEMBER(database_status, irreversible_id)
    STRUCT_MEMBER(database_status, first)
}

// todo: version number argument
/// \exclude
extern "C" void get_database_status(void* cb_alloc_data, void* (*cb_alloc)(void* cb_alloc_data, size_t size));

/// \exclude
template <typename Alloc_fn>
inline void get_database_status(Alloc_fn alloc_fn) {
    get_database_status(&alloc_fn, [](void* cb_alloc_data, size_t size) -> void* { //
        return (*reinterpret_cast<Alloc_fn*>(cb_alloc_data))(size);
    });
}

/// Get the current database status
inline database_status get_database_status() {
    database_status   result;
    std::vector<char> bin;
    get_database_status([&bin](size_t size) {
        bin.resize(size);
        return bin.data();
    });
    datastream<const char*> ds(bin.data(), bin.size());
    ds >> result;
    return result;
}

/// \output_section Query Database
/// Query the database. `request` must be one of the `query_*` structs. Returns result in serialized form.
/// The serialized form is the same as `vector<vector<char>>`'s serialized form. Each inner vector contains the
/// serialized form of a record. The record type varies with query.
///
/// Use `for_each_query_result` or `for_each_contract_row` to iterate through the result.
template <typename T>
inline std::vector<char> query_database(const T& request) {
    std::vector<char> result;
    query_database(request, [&result](size_t size) {
        result.resize(size);
        return result.data();
    });
    return result;
}

/// Unpack each record of a query result and call `f(record)`. `T` is the record type.
template <typename T, typename F>
bool for_each_query_result(const std::vector<char>& bytes, F f) {
    datastream<const char*> ds(bytes.data(), bytes.size());
    unsigned_int            size;
    ds >> size;
    for (uint32_t i = 0; i < size.value; ++i) {
        shared_memory<datastream<const char*>> record{};
        ds >> record;
        T r;
        *record >> r;
        if (!f(r))
            return false;
    }
    return true;
}

/// Use with `query_contract_row_*`. Unpack each row of a query result and call
/// `f(row, data)`. `row` is an instance of `contract_row`. `data` is the unpacked
/// contract-specific data. `T` identifies the type of `data`.
template <typename T, typename F>
bool for_each_contract_row(const std::vector<char>& bytes, F f) {
    return for_each_query_result<contract_row>(bytes, [&](contract_row& row) {
        T p;
        if (row.present && row.value->remaining()) {
            // todo: don't assert if serialization fails
            *row.value >> p;
            if (!f(row, &p))
                return false;
        } else {
            if (!f(row, (T*)nullptr))
                return false;
        }
        return true;
    });
}

/// \exclude
extern "C" void query_database(void* req_begin, void* req_end, void* cb_alloc_data, void* (*cb_alloc)(void* cb_alloc_data, size_t size));

/// \exclude
template <typename T, typename Alloc_fn>
inline void query_database(const T& req, Alloc_fn alloc_fn) {
    auto req_data = pack(req);
    query_database(req_data.data(), req_data.data() + req_data.size(), &alloc_fn, [](void* cb_alloc_data, size_t size) -> void* {
        return (*reinterpret_cast<Alloc_fn*>(cb_alloc_data))(size);
    });
}

} // namespace eosio
