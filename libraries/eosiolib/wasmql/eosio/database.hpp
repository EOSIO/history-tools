// copyright defined in LICENSE.txt

// todo: first vs. first_key, last vs. last_key
// todo: results vs. response vs. rows vs. records

#pragma once

#ifdef GENERATING_DOC
#include <eosio/fixed_bytes.hpp>
#include <eosio/shared_memory.hpp>
#include <eosio/struct_reflection.hpp>
#include <eosio/temp_placeholders.hpp>
#include <type_traits>

#else
#include <eosio/crypto.hpp>
#include <eosio/fixed_bytes.hpp>
#include <eosio/name.hpp>
#include <eosio/shared_memory.hpp>
#include <eosio/struct_reflection.hpp>
#include <eosio/temp_placeholders.hpp>
#include <eosio/time.hpp>
#include <eosio/to_json.hpp>
#include <type_traits>
#endif

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
__attribute__((noinline)) inline eosio::rope to_json(transaction_status value) {
    return eosio::int_to_json(std::underlying_type_t<transaction_status>(value));
}

/// Information extracted from a block
struct block_info {
    uint32_t        block_num             = {};
    checksum256     block_id              = {};
    block_timestamp timestamp             = block_timestamp{};
    name            producer              = {};
    uint16_t        confirmed             = {};
    checksum256     previous              = {};
    checksum256     transaction_mroot     = {};
    checksum256     action_mroot          = {};
    uint32_t        schedule_version      = {};
    uint32_t        new_producers_version = {};
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

/// Details about action execution
struct receipt {
    eosio::name  receiver        = {};
    checksum256  act_digest      = {};
    uint64_t     global_sequence = {};
    uint64_t     recv_sequence   = {};
    unsigned_int code_sequence   = {};
    unsigned_int abi_sequence    = {};

    EOSLIB_SERIALIZE(receipt, (receiver)(act_digest)(global_sequence)(recv_sequence)(code_sequence)(abi_sequence))
};

STRUCT_REFLECT(receipt) {
    STRUCT_MEMBER(receipt, receiver);
    STRUCT_MEMBER(receipt, act_digest);
    STRUCT_MEMBER(receipt, global_sequence);
    STRUCT_MEMBER(receipt, recv_sequence);
    STRUCT_MEMBER(receipt, code_sequence);
    STRUCT_MEMBER(receipt, abi_sequence);
}

/// Details about action execution
struct action {
    eosio::name                            account = {};
    eosio::name                            name    = {};
    shared_memory<datastream<const char*>> data    = {};

    EOSLIB_SERIALIZE(action, (account)(name)(data))
};

STRUCT_REFLECT(action) {
    STRUCT_MEMBER(action, account)
    STRUCT_MEMBER(action, name)
    STRUCT_MEMBER(action, data)
}

/// Details about action execution
struct action_trace {
    uint32_t                  block_num              = {};
    checksum256               transaction_id         = {};
    eosio::transaction_status transaction_status     = {};
    unsigned_int              action_ordinal         = {};
    unsigned_int              creator_action_ordinal = {};
    std::optional<receipt>    receipt                = {};
    eosio::name               receiver               = {};
    struct action             action                 = {};
    bool                      context_free           = {};
    int64_t                   elapsed                = {};
    std::string               console                = {};
    std::string               except                 = {};
    uint64_t                  error_code             = {};

    EOSLIB_SERIALIZE(
        action_trace, (block_num)(transaction_id)(transaction_status)(action_ordinal)(creator_action_ordinal)(receipt)(receiver)(action)(
                          context_free)(elapsed)(console)(except)(error_code))
};

STRUCT_REFLECT(action_trace) {
    STRUCT_MEMBER(action_trace, block_num)
    STRUCT_MEMBER(action_trace, transaction_id)
    STRUCT_MEMBER(action_trace, transaction_status)
    STRUCT_MEMBER(action_trace, action_ordinal)
    STRUCT_MEMBER(action_trace, creator_action_ordinal)
    STRUCT_MEMBER(action_trace, receipt)
    STRUCT_MEMBER(action_trace, receiver)
    STRUCT_MEMBER(action_trace, action)
    STRUCT_MEMBER(action_trace, context_free)
    STRUCT_MEMBER(action_trace, elapsed)
    STRUCT_MEMBER(action_trace, console)
    STRUCT_MEMBER(action_trace, except)
    STRUCT_MEMBER(action_trace, error_code)
}

/// Details about an account
struct account {
    uint32_t                               block_num     = {};
    bool                                   present       = {};
    eosio::name                            name          = {};
    block_timestamp_type                   creation_date = block_timestamp_type{};
    shared_memory<datastream<const char*>> abi           = {};

    EOSLIB_SERIALIZE(account, (block_num)(present)(name)(creation_date)(abi))
};

STRUCT_REFLECT(account) {
    STRUCT_MEMBER(account, block_num)
    STRUCT_MEMBER(account, present)
    STRUCT_MEMBER(account, name)
    STRUCT_MEMBER(account, creation_date)
    STRUCT_MEMBER(account, abi)
}

/// Key for looking up code
struct code_key {
    uint8_t     vm_type    = {};
    uint8_t     vm_version = {};
    checksum256 hash       = {};

    EOSLIB_SERIALIZE(code_key, (vm_type)(vm_version)(hash))
};

/// \exclude
inline std::string_view schema_type_name(code_key*) { return "eosio::code_key"; }

/// \exclude
template <typename F>
void for_each_member(code_key*, F f) {
    STRUCT_MEMBER(code_key, vm_type);
    STRUCT_MEMBER(code_key, vm_version);
    STRUCT_MEMBER(code_key, hash);
}

// todo: reverse direction of join
/// account and account_metadata joined
struct account_metadata_joined {
    uint32_t                               block_num             = {};
    bool                                   present               = {};
    name                                   name                  = {};
    bool                                   privileged            = {};
    time_point                             last_code_update      = time_point{};
    std::optional<code_key>                code                  = {};
    uint32_t                               account_block_num     = {};
    bool                                   account_present       = {}; // todo: switch to optional?
    block_timestamp_type                   account_creation_date = block_timestamp_type{};
    shared_memory<datastream<const char*>> account_abi           = {};

    EOSLIB_SERIALIZE(
        account_metadata_joined, (block_num)(present)(name)(privileged)(last_code_update)(code)(account_block_num)(account_present)(
                                     account_creation_date)(account_abi))
};

/// \exclude
inline std::string_view schema_type_name(account_metadata_joined*) { return "eosio::account_metadata_joined"; }

/// \exclude
template <typename F>
void for_each_member(account_metadata_joined*, F f) {
    STRUCT_MEMBER(account_metadata_joined, block_num);
    STRUCT_MEMBER(account_metadata_joined, present);
    STRUCT_MEMBER(account_metadata_joined, name);
    STRUCT_MEMBER(account_metadata_joined, privileged);
    STRUCT_MEMBER(account_metadata_joined, last_code_update);
    STRUCT_MEMBER(account_metadata_joined, code);
    STRUCT_MEMBER(account_metadata_joined, account_block_num);
    STRUCT_MEMBER(account_metadata_joined, account_present);
    STRUCT_MEMBER(account_metadata_joined, account_creation_date);
    STRUCT_MEMBER(account_metadata_joined, account_abi);
}

/// account_metadata and code joined
struct metadata_code_joined {
    uint32_t                               block_num        = {};
    bool                                   present          = {};
    name                                   name             = {};
    bool                                   privileged       = {};
    time_point                             last_code_update = time_point{};
    std::optional<code_key>                code             = {};
    uint32_t                               join_block_num   = {};
    bool                                   join_present     = {}; // todo: switch to optional?
    uint8_t                                join_vm_type     = {};
    uint8_t                                join_vm_version  = {};
    checksum256                            join_code_hash   = {};
    shared_memory<datastream<const char*>> join_code        = {};

    EOSLIB_SERIALIZE(
        metadata_code_joined,                                                                        //
        (block_num)(present)(name)(privileged)(last_code_update)(code)(join_block_num)(join_present) //
        (join_vm_type)(join_vm_version)(join_code_hash)(join_code))
};

/// \exclude
inline std::string_view schema_type_name(metadata_code_joined*) { return "eosio::metadata_code_joined"; }

/// \exclude
template <typename F>
void for_each_member(metadata_code_joined*, F f) {
    STRUCT_MEMBER(metadata_code_joined, block_num);
    STRUCT_MEMBER(metadata_code_joined, present);
    STRUCT_MEMBER(metadata_code_joined, name);
    STRUCT_MEMBER(metadata_code_joined, privileged);
    STRUCT_MEMBER(metadata_code_joined, last_code_update);
    STRUCT_MEMBER(metadata_code_joined, code);
    STRUCT_MEMBER(metadata_code_joined, join_block_num);
    STRUCT_MEMBER(metadata_code_joined, join_present);
    STRUCT_MEMBER(metadata_code_joined, join_vm_type);
    STRUCT_MEMBER(metadata_code_joined, join_vm_version);
    STRUCT_MEMBER(metadata_code_joined, join_code_hash);
    STRUCT_MEMBER(metadata_code_joined, join_code);
}

/// A row in a contract's table
struct contract_row {
    uint32_t                               block_num   = {};
    bool                                   present     = {};
    name                                   code        = {};
    name                                   scope       = {};
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
    uint32_t                               block_num     = {};
    bool                                   present       = {};
    name                                   code          = {};
    name                                   scope         = {};
    name                                   table         = {};
    uint64_t                               primary_key   = {};
    name                                   payer         = {};
    T                                      secondary_key = {};
    uint32_t                               row_block_num = {};
    bool                                   row_present   = {}; // todo: switch to optional?
    name                                   row_payer     = {};
    shared_memory<datastream<const char*>> row_value     = {};
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

/// Pass this to `query_database` to get `action_trace` for a range of keys. Only includes actions
/// in executed transactions.
///
/// The query results are sorted by `key`. Every record has a different key.
/// ```c++
/// struct key {
///     eosio::name     name             = {};
///     eosio::name     receiver         = {};
///     eosio::name     account          = {};
///     uint32_t        block_num        = {};
///     checksum256     transaction_id   = {};
///     uint32_t        action_ordinal   = {};
///
///     // Construct the key from `data`
///     static key from_data(const action_trace& data);
/// };
/// ```
struct query_action_trace_executed_range_name_receiver_account_block_trans_action {
    struct key {
        eosio::name name           = {};
        eosio::name receiver       = {};
        eosio::name account        = {};
        uint32_t    block_num      = {};
        checksum256 transaction_id = {};
        uint32_t    action_ordinal = {};

        // Extract the key from `data`
        static key from_data(const action_trace& data) {
            return {
                .name           = data.action.name,
                .receiver       = data.receiver,
                .account        = data.action.account,
                .block_num      = data.block_num,
                .transaction_id = data.transaction_id,
                .action_ordinal = data.action_ordinal,
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
    return increment_key(key.action_ordinal) && //
           increment_key(key.transaction_id) && //
           increment_key(key.block_num) &&      //
           increment_key(key.account) &&        //
           increment_key(key.receiver) &&       //
           increment_key(key.name);
}

/// Pass this to `query_database` to get `action_trace` for a range of `receipt_receiver` names.
/// The query results are sorted by `key`.  Every record has a unique key.
/// ```c++
/// struct key {
///     eosio::name     receipt_receiver = {};
///     uint32_t        block_num        = {};
///     checksum256     transaction_id   = {};
///     uint32_t        action_ordinal   = {};
///
///     // Construct the key from `data`
///     static key from_data(const action_trace& data);
/// };
/// ```
struct query_action_trace_receipt_receiver {
    struct key {
        eosio::name receipt_receiver = {};
        uint32_t    block_num        = {};
        checksum256 transaction_id   = {};
        uint32_t    action_ordinal   = {};

        // Extract the key from `data`
        static key from_data(const action_trace& data) {
            return {
                .receipt_receiver = data.receiver,
                .block_num        = data.block_num,
                .transaction_id   = data.transaction_id,
                .action_ordinal   = data.action_ordinal,
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
    return increment_key(key.action_ordinal) && //
           increment_key(key.transaction_id) && //
           increment_key(key.block_num) &&      //
           increment_key(key.receipt_receiver);
}

/// Pass this to `query_database` to get a transaction receipt for a transaction id.
/// The query results are sorted by `key`.  Every record has a unique key.
/// ```c++
/// struct key {
///     checksum256 transaction_id = {};
///     uint32_t block_num = {};
///     uint32_t action_ordinal = {};
///
///     // Construct the key from `data`
///     static key from_data(const action_trace& data);
/// };
/// ```
struct query_transaction_receipt {
    struct key {
        checksum256 transaction_id = {};
        uint32_t    block_num      = {};
        uint32_t    action_ordinal = {};

        // Extract the key from `data`
        static key from_data(const action_trace& data) {
            return {
                .transaction_id = data.transaction_id,
                .block_num      = data.block_num,
                .action_ordinal = data.action_ordinal,
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
    return increment_key(key.action_ordinal) && //
           increment_key(key.block_num) &&      //
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

// todo: reverse direction of join
/// Pass this to `query_database` to get `account_metadata_joined` for a range of names.
/// The query results are sorted by `name`. Every record has a different name.
struct query_acctmeta_range_name {
    /// Identifies query type. Do not modify this field.
    name query_name = "acctmeta.jn"_n;

    /// Look at this point of time in history
    uint32_t max_block = {};

    /// Query records with `name` in the range [`first`, `last`].
    name first = {};

    /// Query records with `name` in the range [`first`, `last`].
    name last = {};

    /// Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.
    uint32_t max_results = {};
};

/// Pass this to `query_database` to get `metadata_code_joined` for a range of names.
/// The query results are sorted by `name`. Every record has a different name.
struct query_code_range_name {
    /// Identifies query type. Do not modify this field.
    name query_name = "meta.jn.code"_n;

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
///     name     scope       = {};
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
        name     scope       = {};

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
///     name     scope       = {};
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
        name     scope       = {};
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
///     name     scope       = {};
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
        name     scope       = {};
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
///     name     scope         = {};
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
        name     scope         = {};
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
