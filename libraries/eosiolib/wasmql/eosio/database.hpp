// copyright defined in LICENSE.txt

// todo: first vs. first_key, last vs. last_key
// todo: results vs. response vs. rows

#pragma once
#include <eosio/struct-reflection.hpp>
#include <eosio/temp-placeholders.hpp>
#include <eosiolib/time.hpp>

namespace eosio {

extern "C" void query_database(void* req_begin, void* req_end, void* cb_alloc_data, void* (*cb_alloc)(void* cb_alloc_data, size_t size));

template <typename T, typename Alloc_fn>
inline void query_database(const T& req, Alloc_fn alloc_fn) {
    auto req_data = pack(req);
    query_database(req_data.data(), req_data.data() + req_data.size(), &alloc_fn, [](void* cb_alloc_data, size_t size) -> void* {
        return (*reinterpret_cast<Alloc_fn*>(cb_alloc_data))(size);
    });
}

template <typename T>
inline std::vector<char> query_database(const T& req) {
    std::vector<char> result;
    query_database(req, [&result](size_t size) {
        result.resize(size);
        return result.data();
    });
    return result;
}

template <typename result, typename F>
bool for_each_query_result(const std::vector<char>& bytes, F f) {
    datastream<const char*> ds(bytes.data(), bytes.size());
    unsigned_int            size;
    ds >> size;
    for (uint32_t i = 0; i < size.value; ++i) {
        datastream<const char*> row{nullptr, 0};
        ds >> row;
        result r;
        row >> r;
        if (!f(r))
            return false;
    }
    return true;
}

struct database_status {
    uint32_t    head            = {};
    checksum256 head_id         = {};
    uint32_t    irreversible    = {};
    checksum256 irreversible_id = {};
    uint32_t    first           = {};
};

STRUCT_REFLECT(database_status) {
    STRUCT_MEMBER(database_status, head)
    STRUCT_MEMBER(database_status, head_id)
    STRUCT_MEMBER(database_status, irreversible)
    STRUCT_MEMBER(database_status, irreversible_id)
    STRUCT_MEMBER(database_status, first)
}

// todo: version number argument
extern "C" void get_database_status(void* cb_alloc_data, void* (*cb_alloc)(void* cb_alloc_data, size_t size));

template <typename Alloc_fn>
inline void get_database_status(Alloc_fn alloc_fn) {
    get_database_status(&alloc_fn, [](void* cb_alloc_data, size_t size) -> void* { //
        return (*reinterpret_cast<Alloc_fn*>(cb_alloc_data))(size);
    });
}

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

// todo: split out definitions useful for client-side
using absolute_block     = tagged_type<"absolute"_n, int32_t>;
using head_block         = tagged_type<"head"_n, int32_t>;
using irreversible_block = tagged_type<"irreversible"_n, int32_t>;
using block_select       = tagged_variant<serialize_tag_as_index, absolute_block, head_block, irreversible_block>;

inline std::string_view schema_type_name(block_select*) { return "eosio::block_select"; }

inline block_select make_absolute_block(int32_t i) {
    block_select result;
    result.value.emplace<0>(i);
    return result;
}

inline uint32_t get_block_num(const block_select& sel, const database_status& status) {
    switch (sel.value.index()) {
    case 0: return std::max((int32_t)0, std::get<0>(sel.value));
    case 1: return std::max((int32_t)0, (int32_t)status.head + std::get<1>(sel.value));
    case 2: return std::max((int32_t)0, (int32_t)status.irreversible + std::get<2>(sel.value));
    default: return 0x7fff'ffff;
    }
}

inline uint32_t increment(block_select& sel) {
    eosio_assert(sel.value.index() == 0, "can only increment absolute block_select");
    int32_t& result = std::get<0>(sel.value);
    result          = uint32_t(result) + 1;
    if (result < 0)
        result = 0;
    return result;
}

struct block_info {
    uint32_t                    block_num             = {};
    serial_wrapper<checksum256> block_id              = {};
    block_timestamp             timestamp             = block_timestamp{};
    name                        producer              = {};
    uint16_t                    confirmed             = {};
    serial_wrapper<checksum256> previous              = {};
    serial_wrapper<checksum256> transaction_mroot     = {};
    serial_wrapper<checksum256> action_mroot          = {};
    uint32_t                    schedule_version      = {};
    uint32_t                    new_producers_version = {};
    // std::vector<producer_key>       new_producers         = {}; // todo
};

STRUCT_REFLECT(eosio::block_info) {
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

struct query_block_info_range_index {
    name     query_name  = "block.info"_n;
    uint32_t first       = {};
    uint32_t last        = {};
    uint32_t max_results = {};
};

enum class transaction_status : uint8_t {
    executed  = 0, // succeed, no error handler executed
    soft_fail = 1, // objectively failed (not executed), error handler executed
    hard_fail = 2, // objectively failed and error handler objectively failed thus no state change
    delayed   = 3, // transaction delayed/deferred/scheduled for future execution
    expired   = 4, // transaction expired and storage space refunded to user
};

struct action_trace {
    uint32_t                    block_index             = {};
    serial_wrapper<checksum256> transaction_id          = {};
    uint32_t                    action_index            = {};
    uint32_t                    parent_action_index     = {};
    eosio::transaction_status   transaction_status      = {};
    eosio::name                 receipt_receiver        = {};
    serial_wrapper<checksum256> receipt_act_digest      = {};
    uint64_t                    receipt_global_sequence = {};
    uint64_t                    receipt_recv_sequence   = {};
    unsigned_int                receipt_code_sequence   = {};
    unsigned_int                receipt_abi_sequence    = {};
    eosio::name                 account                 = {};
    eosio::name                 name                    = {};
    datastream<const char*>     data                    = {nullptr, 0};
    bool                        context_free            = {};
    int64_t                     elapsed                 = {};

    EOSLIB_SERIALIZE(
        action_trace, (block_index)(transaction_id)(action_index)(parent_action_index)(transaction_status)(receipt_receiver)(
                          receipt_act_digest)(receipt_global_sequence)(receipt_recv_sequence)(receipt_code_sequence)(receipt_abi_sequence)(
                          account)(name)(data)(context_free)(elapsed))
};

inline std::string_view schema_type_name(action_trace*) { return "eosio::action_trace"; }

struct query_action_trace_executed_range_name_receiver_account_block_trans_action {
    struct key {
        eosio::name                 name             = {};
        eosio::name                 receipt_receiver = {};
        eosio::name                 account          = {};
        uint32_t                    block_index      = {};
        serial_wrapper<checksum256> transaction_id   = {};
        uint32_t                    action_index     = {};
    };

    name     query_name  = "at.e.nra"_n;
    uint32_t max_block   = {};
    key      first       = {};
    key      last        = {};
    uint32_t max_results = {};
};

struct account {
    uint32_t                    block_index      = {};
    bool                        present          = {};
    eosio::name                 name             = {};
    uint8_t                     vm_type          = {};
    uint8_t                     vm_version       = {};
    bool                        privileged       = {};
    time_point                  last_code_update = time_point{};
    serial_wrapper<checksum256> code_version     = {};
    block_timestamp_type        creation_date    = block_timestamp_type{};
    datastream<const char*>     code             = {nullptr, 0};
    datastream<const char*>     abi              = {nullptr, 0};

    EOSLIB_SERIALIZE(
        account, (block_index)(present)(name)(vm_type)(vm_version)(privileged)(last_code_update)(code_version)(creation_date)(code)(abi))
};

STRUCT_REFLECT(eosio::account) {
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

struct query_account_range_name {
    name     query_name  = "account"_n;
    uint32_t max_block   = {};
    name     first       = {};
    name     last        = {};
    uint32_t max_results = {};
};

struct contract_row {
    uint32_t                block_index = {};
    bool                    present     = {};
    name                    code        = {};
    uint64_t                scope       = {};
    name                    table       = {};
    uint64_t                primary_key = {};
    name                    payer       = {};
    datastream<const char*> value       = {nullptr, 0};
};

inline std::string_view schema_type_name(contract_row*) { return "eosio::contract_row"; }

template <typename payload, typename F>
bool for_each_contract_row(const std::vector<char>& bytes, F f) {
    return for_each_query_result<contract_row>(bytes, [&](contract_row& row) {
        payload p;
        if (row.present && row.value.remaining()) {
            // todo: don't assert if serialization fails
            row.value >> p;
            if (!f(row, &p))
                return false;
        } else {
            if (!f(row, nullptr))
                return false;
        }
        return true;
    });
}

struct query_contract_row_range_code_table_pk_scope {
    struct key {
        name     code        = {};
        name     table       = {};
        uint64_t primary_key = {};
        uint64_t scope       = {};
    };

    name     query_name  = "cr.ctps"_n;
    uint32_t max_block   = {};
    key      first       = {};
    key      last        = {};
    uint32_t max_results = {};
};

struct query_contract_row_range_code_table_scope_pk {
    struct key {
        name     code        = {};
        name     table       = {};
        uint64_t scope       = {};
        uint64_t primary_key = {};
    };

    name     query_name  = "cr.ctsp"_n;
    uint32_t max_block   = {};
    key      first       = {};
    key      last        = {};
    uint32_t max_results = {};
};

struct query_contract_row_range_scope_table_pk_code {
    struct key {
        uint64_t scope       = {};
        name     table       = {};
        uint64_t primary_key = {};
        name     code        = {};
    };

    name     query_name  = "cr.stpc"_n;
    uint32_t max_block   = {};
    key      first       = {};
    key      last        = {};
    uint32_t max_results = {};
};

template <typename T>
struct contract_secondary_index_with_row {
    uint32_t                block_index     = {};
    bool                    present         = {};
    name                    code            = {};
    uint64_t                scope           = {};
    name                    table           = {};
    uint64_t                primary_key     = {};
    name                    payer           = {};
    T                       secondary_key   = {};
    uint32_t                row_block_index = {};
    bool                    row_present     = {};
    name                    row_payer       = {};
    datastream<const char*> row_value       = {nullptr, 0};
};

// todo: for_each_...

struct query_contract_index64_range_code_table_scope_sk_pk {
    struct key {
        name     code          = {};
        name     table         = {};
        uint64_t scope         = {};
        uint64_t secondary_key = {};
        uint64_t primary_key   = {};
    };

    name     query_name  = "ci1.cts2p"_n;
    uint32_t max_block   = {};
    key      first       = {};
    key      last        = {};
    uint32_t max_results = {};
};

} // namespace eosio
