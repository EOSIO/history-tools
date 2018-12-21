// copyright defined in LICENSE.txt

#pragma once
#include "lib-placeholders.hpp"

extern "C" void exec_query(void* req_begin, void* req_end, void* cb_alloc_data, void* (*cb_alloc)(void* cb_alloc_data, size_t size));

template <typename T, typename Alloc_fn>
inline void exec_query(const T& req, Alloc_fn alloc_fn) {
    auto req_data = eosio::pack(req);
    exec_query(req_data.data(), req_data.data() + req_data.size(), &alloc_fn, [](void* cb_alloc_data, size_t size) -> void* {
        return (*reinterpret_cast<Alloc_fn*>(cb_alloc_data))(size);
    });
}

template <typename T>
inline std::vector<char> exec_query(const T& req) {
    std::vector<char> result;
    exec_query(req, [&result](size_t size) {
        result.resize(size);
        return result.data();
    });
    return result;
}

template <typename result, typename F>
bool for_each_query_result(const std::vector<char>& bytes, F f) {
    eosio::datastream<const char*> ds(bytes.data(), bytes.size());
    unsigned_int                   size;
    ds >> size;
    for (uint32_t i = 0; i < size.value; ++i) {
        eosio::datastream<const char*> row{nullptr, 0};
        ds >> row;
        result r;
        row >> r;
        if (!f(r))
            return false;
    }
    return true;
}

enum class transaction_status : uint8_t {
    executed  = 0, // succeed, no error handler executed
    soft_fail = 1, // objectively failed (not executed), error handler executed
    hard_fail = 2, // objectively failed and error handler objectively failed thus no state change
    delayed   = 3, // transaction delayed/deferred/scheduled for future execution
    expired   = 4, // transaction expired and storage space refunded to user
};

struct action_trace {
    uint32_t                           block_index             = {};
    serial_wrapper<eosio::checksum256> transaction_id          = {};
    uint32_t                           action_index            = {};
    uint32_t                           parent_action_index     = {};
    ::transaction_status               transaction_status      = {};
    eosio::name                        receipt_receiver        = {};
    serial_wrapper<eosio::checksum256> receipt_act_digest      = {};
    uint64_t                           receipt_global_sequence = {};
    uint64_t                           receipt_recv_sequence   = {};
    unsigned_int                       receipt_code_sequence   = {};
    unsigned_int                       receipt_abi_sequence    = {};
    eosio::name                        account                 = {};
    eosio::name                        name                    = {};
    eosio::datastream<const char*>     data                    = {nullptr, 0};
    bool                               context_free            = {};
    int64_t                            elapsed                 = {};

    EOSLIB_SERIALIZE(
        action_trace, (block_index)(transaction_id)(action_index)(parent_action_index)(transaction_status)(receipt_receiver)(
                          receipt_act_digest)(receipt_global_sequence)(receipt_recv_sequence)(receipt_code_sequence)(receipt_abi_sequence)(
                          account)(name)(data)(context_free)(elapsed))
};

struct query_action_trace_executed_range_name_receiver_account_block_trans_action {
    struct key {
        eosio::name                        name             = {};
        eosio::name                        receipt_receiver = {};
        eosio::name                        account          = {};
        uint32_t                           block_index      = {};
        serial_wrapper<eosio::checksum256> transaction_id   = {};
        uint32_t                           action_index     = {};
    };

    eosio::name query_name      = "at.e.nra"_n;
    uint32_t    max_block_index = {};
    key         first           = {};
    key         last            = {};
    uint32_t    max_results     = {};
};

struct contract_row {
    uint32_t                       block_index = {};
    bool                           present     = {};
    eosio::name                    code        = {};
    uint64_t                       scope       = {};
    eosio::name                    table       = {};
    uint64_t                       primary_key = {};
    eosio::name                    payer       = {};
    eosio::datastream<const char*> value       = {nullptr, 0};
};

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
        eosio::name code        = {};
        eosio::name table       = {};
        uint64_t    primary_key = {};
        uint64_t    scope       = {};
    };

    eosio::name query_name      = "cr.ctps"_n;
    uint32_t    max_block_index = {};
    key         first           = {};
    key         last            = {};
    uint32_t    max_results     = {};
};

struct query_contract_row_range_code_table_scope_pk {
    struct key {
        eosio::name code        = {};
        eosio::name table       = {};
        uint64_t    scope       = {};
        uint64_t    primary_key = {};
    };

    eosio::name query_name      = "cr.ctsp"_n;
    uint32_t    max_block_index = {};
    key         first           = {};
    key         last            = {};
    uint32_t    max_results     = {};
};

struct query_contract_row_range_scope_table_pk_code {
    struct key {
        uint64_t    scope       = {};
        eosio::name table       = {};
        uint64_t    primary_key = {};
        eosio::name code        = {};
    };

    eosio::name query_name      = "cr.stpc"_n;
    uint32_t    max_block_index = {};
    key         first           = {};
    key         last            = {};
    uint32_t    max_results     = {};
};
