// copyright defined in LICENSE.txt

#pragma once
#include "abieos_exception.hpp"

namespace state_history {

struct extension {
    uint16_t             type = {};
    abieos::input_buffer data = {};
};

ABIEOS_REFLECT(extension) {
    ABIEOS_MEMBER(extension, type)
    ABIEOS_MEMBER(extension, data)
}

struct fill_status {
    uint32_t            head            = {};
    abieos::checksum256 head_id         = {};
    uint32_t            irreversible    = {};
    abieos::checksum256 irreversible_id = {};
    uint32_t            first           = {};
};

ABIEOS_REFLECT(fill_status) {
    ABIEOS_MEMBER(fill_status, head)
    ABIEOS_MEMBER(fill_status, head_id)
    ABIEOS_MEMBER(fill_status, irreversible)
    ABIEOS_MEMBER(fill_status, irreversible_id)
    ABIEOS_MEMBER(fill_status, first)
}

inline bool operator==(const fill_status& a, fill_status& b) {
    return std::tie(a.head, a.head_id, a.irreversible, a.irreversible_id, a.first) ==
           std::tie(b.head, b.head_id, b.irreversible, b.irreversible_id, b.first);
}

inline bool operator!=(const fill_status& a, fill_status& b) { return !(a == b); }

enum class transaction_status : uint8_t {
    executed  = 0, // succeed, no error handler executed
    soft_fail = 1, // objectively failed (not executed), error handler executed
    hard_fail = 2, // objectively failed and error handler objectively failed thus no state change
    delayed   = 3, // transaction delayed/deferred/scheduled for future execution
    expired   = 4, // transaction expired and storage space refunded to user
};

inline std::string to_string(transaction_status status) {
    switch (status) {
    case transaction_status::executed: return "executed";
    case transaction_status::soft_fail: return "soft_fail";
    case transaction_status::hard_fail: return "hard_fail";
    case transaction_status::delayed: return "delayed";
    case transaction_status::expired: return "expired";
    }
    throw std::runtime_error("unknown status: " + std::to_string((uint8_t)status));
}

inline bool bin_to_native(transaction_status& status, abieos::bin_to_native_state& state, bool) {
    status = transaction_status(abieos::read_raw<uint8_t>(state.bin));
    return true;
}

inline bool json_to_native(transaction_status&, abieos::json_to_native_state&, abieos::event_type, bool) {
    throw abieos::error("json_to_native: transaction_status unsupported");
}

struct block_position {
    uint32_t            block_num = {};
    abieos::checksum256 block_id  = {};
};

ABIEOS_REFLECT(block_position) {
    ABIEOS_MEMBER(block_position, block_num)
    ABIEOS_MEMBER(block_position, block_id)
}

struct get_blocks_result_v0 {
    block_position                      head              = {};
    block_position                      last_irreversible = {};
    std::optional<block_position>       this_block        = {};
    std::optional<block_position>       prev_block        = {};
    std::optional<abieos::input_buffer> block             = {};
    std::optional<abieos::input_buffer> traces            = {};
    std::optional<abieos::input_buffer> deltas            = {};
};

ABIEOS_REFLECT(get_blocks_result_v0) {
    ABIEOS_MEMBER(get_blocks_result_v0, head)
    ABIEOS_MEMBER(get_blocks_result_v0, last_irreversible)
    ABIEOS_MEMBER(get_blocks_result_v0, this_block)
    ABIEOS_MEMBER(get_blocks_result_v0, prev_block)
    ABIEOS_MEMBER(get_blocks_result_v0, block)
    ABIEOS_MEMBER(get_blocks_result_v0, traces)
    ABIEOS_MEMBER(get_blocks_result_v0, deltas)
}

struct row {
    bool                 present = {};
    abieos::input_buffer data    = {};
};

ABIEOS_REFLECT(row) {
    ABIEOS_MEMBER(row, present)
    ABIEOS_MEMBER(row, data)
}

struct table_delta_v0 {
    std::string      name = {};
    std::vector<row> rows = {};
};

ABIEOS_REFLECT(table_delta_v0) {
    ABIEOS_MEMBER(table_delta_v0, name)
    ABIEOS_MEMBER(table_delta_v0, rows)
}

struct permission_level {
    abieos::name actor      = {};
    abieos::name permission = {};
};

ABIEOS_REFLECT(permission_level) {
    ABIEOS_MEMBER(permission_level, actor)
    ABIEOS_MEMBER(permission_level, permission)
}

struct account_auth_sequence {
    abieos::name account  = {};
    uint64_t     sequence = {};
};

ABIEOS_REFLECT(account_auth_sequence) {
    ABIEOS_MEMBER(account_auth_sequence, account)
    ABIEOS_MEMBER(account_auth_sequence, sequence)
}

struct account_delta {
    abieos::name account = {};
    int64_t      delta   = {};
};

ABIEOS_REFLECT(account_delta) {
    ABIEOS_MEMBER(account_delta, account)
    ABIEOS_MEMBER(account_delta, delta)
}

struct action_receipt_v0 {
    abieos::name                       receiver        = {};
    abieos::checksum256                act_digest      = {};
    uint64_t                           global_sequence = {};
    uint64_t                           recv_sequence   = {};
    std::vector<account_auth_sequence> auth_sequence   = {};
    abieos::varuint32                  code_sequence   = {};
    abieos::varuint32                  abi_sequence    = {};
};

ABIEOS_REFLECT(action_receipt_v0) {
    ABIEOS_MEMBER(action_receipt_v0, receiver)
    ABIEOS_MEMBER(action_receipt_v0, act_digest)
    ABIEOS_MEMBER(action_receipt_v0, global_sequence)
    ABIEOS_MEMBER(action_receipt_v0, recv_sequence)
    ABIEOS_MEMBER(action_receipt_v0, auth_sequence)
    ABIEOS_MEMBER(action_receipt_v0, code_sequence)
    ABIEOS_MEMBER(action_receipt_v0, abi_sequence)
}

using action_receipt = std::variant<action_receipt_v0>;

struct action {
    abieos::name                  account       = {};
    abieos::name                  name          = {};
    std::vector<permission_level> authorization = {};
    abieos::input_buffer          data          = {};
};

ABIEOS_REFLECT(action) {
    ABIEOS_MEMBER(action, account)
    ABIEOS_MEMBER(action, name)
    ABIEOS_MEMBER(action, authorization)
    ABIEOS_MEMBER(action, data)
}

struct action_trace_v0 {
    abieos::varuint32             action_ordinal         = {};
    abieos::varuint32             creator_action_ordinal = {};
    std::optional<action_receipt> receipt                = {};
    abieos::name                  receiver               = {};
    action                        act                    = {};
    bool                          context_free           = {};
    int64_t                       elapsed                = {};
    std::string                   console                = {};
    std::vector<account_delta>    account_ram_deltas     = {};
    std::optional<std::string>    except                 = {};
    std::optional<uint64_t>       error_code             = {};
};

ABIEOS_REFLECT(action_trace_v0) {
    ABIEOS_MEMBER(action_trace_v0, action_ordinal)
    ABIEOS_MEMBER(action_trace_v0, creator_action_ordinal)
    ABIEOS_MEMBER(action_trace_v0, receipt)
    ABIEOS_MEMBER(action_trace_v0, receiver)
    ABIEOS_MEMBER(action_trace_v0, act)
    ABIEOS_MEMBER(action_trace_v0, context_free)
    ABIEOS_MEMBER(action_trace_v0, elapsed)
    ABIEOS_MEMBER(action_trace_v0, console)
    ABIEOS_MEMBER(action_trace_v0, account_ram_deltas)
    ABIEOS_MEMBER(action_trace_v0, except)
    ABIEOS_MEMBER(action_trace_v0, error_code)
}

using action_trace = std::variant<action_trace_v0>;

struct partial_transaction_v0 {
    abieos::time_point_sec            expiration             = {};
    uint16_t                          ref_block_num          = {};
    uint32_t                          ref_block_prefix       = {};
    abieos::varuint32                 max_net_usage_words    = {};
    uint8_t                           max_cpu_usage_ms       = {};
    abieos::varuint32                 delay_sec              = {};
    std::vector<extension>            transaction_extensions = {};
    std::vector<abieos::signature>    signatures             = {};
    std::vector<abieos::input_buffer> context_free_data      = {};
};

ABIEOS_REFLECT(partial_transaction_v0) {
    ABIEOS_MEMBER(partial_transaction_v0, expiration)
    ABIEOS_MEMBER(partial_transaction_v0, ref_block_num)
    ABIEOS_MEMBER(partial_transaction_v0, ref_block_prefix)
    ABIEOS_MEMBER(partial_transaction_v0, max_net_usage_words)
    ABIEOS_MEMBER(partial_transaction_v0, max_cpu_usage_ms)
    ABIEOS_MEMBER(partial_transaction_v0, delay_sec)
    ABIEOS_MEMBER(partial_transaction_v0, transaction_extensions)
    ABIEOS_MEMBER(partial_transaction_v0, signatures)
    ABIEOS_MEMBER(partial_transaction_v0, context_free_data)
}

using partial_transaction = std::variant<partial_transaction_v0>;

struct recurse_transaction_trace;

struct transaction_trace_v0 {
    abieos::checksum256                    id                = {};
    transaction_status                     status            = {};
    uint32_t                               cpu_usage_us      = {};
    abieos::varuint32                      net_usage_words   = {};
    int64_t                                elapsed           = {};
    uint64_t                               net_usage         = {};
    bool                                   scheduled         = {};
    std::vector<action_trace>              action_traces     = {};
    std::optional<account_delta>           account_ram_delta = {};
    std::optional<std::string>             except            = {};
    std::optional<uint64_t>                error_code        = {};
    std::vector<recurse_transaction_trace> failed_dtrx_trace = {};
    std::optional<partial_transaction>     partial           = {};
};

ABIEOS_REFLECT(transaction_trace_v0) {
    ABIEOS_MEMBER(transaction_trace_v0, id)
    ABIEOS_MEMBER(transaction_trace_v0, status)
    ABIEOS_MEMBER(transaction_trace_v0, cpu_usage_us)
    ABIEOS_MEMBER(transaction_trace_v0, net_usage_words)
    ABIEOS_MEMBER(transaction_trace_v0, elapsed)
    ABIEOS_MEMBER(transaction_trace_v0, net_usage)
    ABIEOS_MEMBER(transaction_trace_v0, scheduled)
    ABIEOS_MEMBER(transaction_trace_v0, action_traces)
    ABIEOS_MEMBER(transaction_trace_v0, account_ram_delta)
    ABIEOS_MEMBER(transaction_trace_v0, except)
    ABIEOS_MEMBER(transaction_trace_v0, error_code)
    ABIEOS_MEMBER(transaction_trace_v0, failed_dtrx_trace)
    ABIEOS_MEMBER(transaction_trace_v0, partial)
}

using transaction_trace = std::variant<transaction_trace_v0>;

struct recurse_transaction_trace {
    transaction_trace recurse = {};
};

inline bool bin_to_native(recurse_transaction_trace& obj, abieos::bin_to_native_state& state, bool start) {
    return abieos::bin_to_native(obj.recurse, state, start);
}

inline bool json_to_native(recurse_transaction_trace& obj, abieos::json_to_native_state& state, abieos::event_type event, bool start) {
    return abieos::json_to_native(obj.recurse, state, event, start);
}

struct producer_key {
    abieos::name       producer_name     = {};
    abieos::public_key block_signing_key = {};
};

ABIEOS_REFLECT(producer_key) {
    ABIEOS_MEMBER(producer_key, producer_name)
    ABIEOS_MEMBER(producer_key, block_signing_key)
}

struct producer_schedule {
    uint32_t                  version   = {};
    std::vector<producer_key> producers = {};
};

ABIEOS_REFLECT(producer_schedule) {
    ABIEOS_MEMBER(producer_schedule, version)
    ABIEOS_MEMBER(producer_schedule, producers)
}

struct transaction_receipt_header {
    transaction_status status          = {};
    uint32_t           cpu_usage_us    = {};
    abieos::varuint32  net_usage_words = {};
};

ABIEOS_REFLECT(transaction_receipt_header) {
    ABIEOS_MEMBER(transaction_receipt_header, status)
    ABIEOS_MEMBER(transaction_receipt_header, cpu_usage_us)
    ABIEOS_MEMBER(transaction_receipt_header, net_usage_words)
}

struct packed_transaction {
    std::vector<abieos::signature> signatures               = {};
    uint8_t                        compression              = {};
    abieos::input_buffer           packed_context_free_data = {};
    abieos::input_buffer           packed_trx               = {};
};

ABIEOS_REFLECT(packed_transaction) {
    ABIEOS_MEMBER(packed_transaction, signatures)
    ABIEOS_MEMBER(packed_transaction, compression)
    ABIEOS_MEMBER(packed_transaction, packed_context_free_data)
    ABIEOS_MEMBER(packed_transaction, packed_trx)
}

using transaction_variant = std::variant<abieos::checksum256, packed_transaction>;

struct transaction_receipt : transaction_receipt_header {
    transaction_variant trx = {};
};

ABIEOS_REFLECT(transaction_receipt) {
    ABIEOS_BASE(transaction_receipt_header)
    ABIEOS_MEMBER(transaction_receipt, trx)
}

struct block_header {
    abieos::block_timestamp          timestamp         = {};
    abieos::name                     producer          = {};
    uint16_t                         confirmed         = {};
    abieos::checksum256              previous          = {};
    abieos::checksum256              transaction_mroot = {};
    abieos::checksum256              action_mroot      = {};
    uint32_t                         schedule_version  = {};
    std::optional<producer_schedule> new_producers     = {};
    std::vector<extension>           header_extensions = {};
};

ABIEOS_REFLECT(block_header) {
    ABIEOS_MEMBER(block_header, timestamp)
    ABIEOS_MEMBER(block_header, producer)
    ABIEOS_MEMBER(block_header, confirmed)
    ABIEOS_MEMBER(block_header, previous)
    ABIEOS_MEMBER(block_header, transaction_mroot)
    ABIEOS_MEMBER(block_header, action_mroot)
    ABIEOS_MEMBER(block_header, schedule_version)
    ABIEOS_MEMBER(block_header, new_producers)
    ABIEOS_MEMBER(block_header, header_extensions)
}

struct signed_block_header : block_header {
    abieos::signature producer_signature = {};
};

ABIEOS_REFLECT(signed_block_header) {
    ABIEOS_BASE(block_header)
    ABIEOS_MEMBER(signed_block_header, producer_signature)
}

struct signed_block : signed_block_header {
    std::vector<transaction_receipt> transactions     = {};
    std::vector<extension>           block_extensions = {};
};

ABIEOS_REFLECT(signed_block) {
    ABIEOS_BASE(signed_block_header)
    ABIEOS_MEMBER(signed_block, transactions)
    ABIEOS_MEMBER(signed_block, block_extensions)
}

} // namespace state_history
