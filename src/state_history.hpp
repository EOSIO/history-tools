// copyright defined in LICENSE.txt

#pragma once
#include "abieos_exception.hpp"

namespace state_history {

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

struct variant_header_zero {};

inline bool bin_to_native(variant_header_zero&, abieos::bin_to_native_state& state, bool) {
    if (read_varuint32(state.bin))
        throw std::runtime_error("unexpected variant value");
    return true;
}

inline bool json_to_native(variant_header_zero&, abieos::json_to_native_state&, abieos::event_type, bool) { return true; }

struct block_position {
    uint32_t            block_num = 0;
    abieos::checksum256 block_id  = {};
};

template <typename F>
constexpr void for_each_field(block_position*, F f) {
    f("block_num", abieos::member_ptr<&block_position::block_num>{});
    f("block_id", abieos::member_ptr<&block_position::block_id>{});
}

struct get_blocks_result_v0 {
    block_position                      head;
    block_position                      last_irreversible;
    std::optional<block_position>       this_block;
    std::optional<block_position>       prev_block;
    std::optional<abieos::input_buffer> block;
    std::optional<abieos::input_buffer> traces;
    std::optional<abieos::input_buffer> deltas;
};

template <typename F>
constexpr void for_each_field(get_blocks_result_v0*, F f) {
    f("head", abieos::member_ptr<&get_blocks_result_v0::head>{});
    f("last_irreversible", abieos::member_ptr<&get_blocks_result_v0::last_irreversible>{});
    f("this_block", abieos::member_ptr<&get_blocks_result_v0::this_block>{});
    f("prev_block", abieos::member_ptr<&get_blocks_result_v0::prev_block>{});
    f("block", abieos::member_ptr<&get_blocks_result_v0::block>{});
    f("traces", abieos::member_ptr<&get_blocks_result_v0::traces>{});
    f("deltas", abieos::member_ptr<&get_blocks_result_v0::deltas>{});
}

struct row {
    bool                 present;
    abieos::input_buffer data;
};

template <typename F>
constexpr void for_each_field(row*, F f) {
    f("present", abieos::member_ptr<&row::present>{});
    f("data", abieos::member_ptr<&row::data>{});
}

struct table_delta_v0 {
    std::string      name;
    std::vector<row> rows;
};

template <typename F>
constexpr void for_each_field(table_delta_v0*, F f) {
    f("name", abieos::member_ptr<&table_delta_v0::name>{});
    f("rows", abieos::member_ptr<&table_delta_v0::rows>{});
}

struct action_trace_authorization {
    abieos::name actor;
    abieos::name permission;
};

template <typename F>
constexpr void for_each_field(action_trace_authorization*, F f) {
    f("actor", abieos::member_ptr<&action_trace_authorization::actor>{});
    f("permission", abieos::member_ptr<&action_trace_authorization::permission>{});
}

struct action_trace_auth_sequence {
    abieos::name account;
    uint64_t     sequence;
};

template <typename F>
constexpr void for_each_field(action_trace_auth_sequence*, F f) {
    f("account", abieos::member_ptr<&action_trace_auth_sequence::account>{});
    f("sequence", abieos::member_ptr<&action_trace_auth_sequence::sequence>{});
}

struct action_trace_ram_delta {
    abieos::name account;
    int64_t      delta;
};

template <typename F>
constexpr void for_each_field(action_trace_ram_delta*, F f) {
    f("account", abieos::member_ptr<&action_trace_ram_delta::account>{});
    f("delta", abieos::member_ptr<&action_trace_ram_delta::delta>{});
}

struct recurse_action_trace;

struct action_trace {
    variant_header_zero                     dummy;
    variant_header_zero                     receipt_dummy;
    abieos::name                            receipt_receiver;
    abieos::checksum256                     receipt_act_digest;
    uint64_t                                receipt_global_sequence;
    uint64_t                                receipt_recv_sequence;
    std::vector<action_trace_auth_sequence> receipt_auth_sequence;
    abieos::varuint32                       receipt_code_sequence;
    abieos::varuint32                       receipt_abi_sequence;
    abieos::name                            account;
    abieos::name                            name;
    std::vector<action_trace_authorization> authorization;
    abieos::input_buffer                    data;
    bool                                    context_free;
    int64_t                                 elapsed;
    std::string                             console;
    std::vector<action_trace_ram_delta>     account_ram_deltas;
    std::optional<std::string>              except;
    std::vector<recurse_action_trace>       inline_traces;
};

template <typename F>
constexpr void for_each_field(action_trace*, F f) {
    f("dummy", abieos::member_ptr<&action_trace::dummy>{});
    f("receipt_dummy", abieos::member_ptr<&action_trace::receipt_dummy>{});
    f("receipt_receiver", abieos::member_ptr<&action_trace::receipt_receiver>{});
    f("receipt_act_digest", abieos::member_ptr<&action_trace::receipt_act_digest>{});
    f("receipt_global_sequence", abieos::member_ptr<&action_trace::receipt_global_sequence>{});
    f("receipt_recv_sequence", abieos::member_ptr<&action_trace::receipt_recv_sequence>{});
    f("receipt_auth_sequence", abieos::member_ptr<&action_trace::receipt_auth_sequence>{});
    f("receipt_code_sequence", abieos::member_ptr<&action_trace::receipt_code_sequence>{});
    f("receipt_abi_sequence", abieos::member_ptr<&action_trace::receipt_abi_sequence>{});
    f("account", abieos::member_ptr<&action_trace::account>{});
    f("name", abieos::member_ptr<&action_trace::name>{});
    f("authorization", abieos::member_ptr<&action_trace::authorization>{});
    f("data", abieos::member_ptr<&action_trace::data>{});
    f("context_free", abieos::member_ptr<&action_trace::context_free>{});
    f("elapsed", abieos::member_ptr<&action_trace::elapsed>{});
    f("console", abieos::member_ptr<&action_trace::console>{});
    f("account_ram_deltas", abieos::member_ptr<&action_trace::account_ram_deltas>{});
    f("except", abieos::member_ptr<&action_trace::except>{});
    f("inline_traces", abieos::member_ptr<&action_trace::inline_traces>{});
}

struct recurse_action_trace : action_trace {};

inline bool bin_to_native(recurse_action_trace& obj, abieos::bin_to_native_state& state, bool start) {
    action_trace& o = obj;
    return bin_to_native(o, state, start);
}

inline bool json_to_native(recurse_action_trace& obj, abieos::json_to_native_state& state, abieos::event_type event, bool start) {
    action_trace& o = obj;
    return json_to_native(o, state, event, start);
}

struct recurse_transaction_trace;

struct transaction_trace {
    variant_header_zero                    dummy;
    abieos::checksum256                    id;
    transaction_status                     status;
    uint32_t                               cpu_usage_us;
    abieos::varuint32                      net_usage_words;
    int64_t                                elapsed;
    uint64_t                               net_usage;
    bool                                   scheduled;
    std::vector<action_trace>              action_traces;
    std::optional<std::string>             except;
    std::vector<recurse_transaction_trace> failed_dtrx_trace;
};

template <typename F>
constexpr void for_each_field(transaction_trace*, F f) {
    f("dummy", abieos::member_ptr<&transaction_trace::dummy>{});
    f("transaction_id", abieos::member_ptr<&transaction_trace::id>{});
    f("status", abieos::member_ptr<&transaction_trace::status>{});
    f("cpu_usage_us", abieos::member_ptr<&transaction_trace::cpu_usage_us>{});
    f("net_usage_words", abieos::member_ptr<&transaction_trace::net_usage_words>{});
    f("elapsed", abieos::member_ptr<&transaction_trace::elapsed>{});
    f("net_usage", abieos::member_ptr<&transaction_trace::net_usage>{});
    f("scheduled", abieos::member_ptr<&transaction_trace::scheduled>{});
    f("action_traces", abieos::member_ptr<&transaction_trace::action_traces>{});
    f("except", abieos::member_ptr<&transaction_trace::except>{});
    f("failed_dtrx_trace", abieos::member_ptr<&transaction_trace::failed_dtrx_trace>{});
}

struct recurse_transaction_trace : transaction_trace {};

inline bool bin_to_native(recurse_transaction_trace& obj, abieos::bin_to_native_state& state, bool start) {
    transaction_trace& o = obj;
    return abieos::bin_to_native(o, state, start);
}

inline bool json_to_native(recurse_transaction_trace& obj, abieos::json_to_native_state& state, abieos::event_type event, bool start) {
    transaction_trace& o = obj;
    return abieos::json_to_native(o, state, event, start);
}

struct producer_key {
    abieos::name       producer_name;
    abieos::public_key block_signing_key;
};

template <typename F>
constexpr void for_each_field(producer_key*, F f) {
    f("producer_name", abieos::member_ptr<&producer_key::producer_name>{});
    f("block_signing_key", abieos::member_ptr<&producer_key::block_signing_key>{});
}

struct extension {
    uint16_t      type;
    abieos::bytes data;
};

template <typename F>
constexpr void for_each_field(extension*, F f) {
    f("type", abieos::member_ptr<&extension::type>{});
    f("data", abieos::member_ptr<&extension::data>{});
}

struct producer_schedule {
    uint32_t                  version   = {};
    std::vector<producer_key> producers = {};
};

template <typename F>
constexpr void for_each_field(producer_schedule*, F f) {
    f("version", abieos::member_ptr<&producer_schedule::version>{});
    f("producers", abieos::member_ptr<&producer_schedule::producers>{});
}

struct transaction_receipt_header {
    uint8_t           status;
    uint32_t          cpu_usage_us;
    abieos::varuint32 net_usage_words;
};

template <typename F>
constexpr void for_each_field(transaction_receipt_header*, F f) {
    f("status", abieos::member_ptr<&transaction_receipt_header::status>{});
    f("cpu_usage_us", abieos::member_ptr<&transaction_receipt_header::cpu_usage_us>{});
    f("net_usage_words", abieos::member_ptr<&transaction_receipt_header::net_usage_words>{});
}

struct packed_transaction {
    std::vector<abieos::signature> signatures;
    uint8_t                        compression;
    abieos::bytes                  packed_context_free_data;
    abieos::bytes                  packed_trx;
};

template <typename F>
constexpr void for_each_field(packed_transaction*, F f) {
    f("signatures", abieos::member_ptr<&packed_transaction::signatures>{});
    f("compression", abieos::member_ptr<&packed_transaction::compression>{});
    f("packed_context_free_data", abieos::member_ptr<&packed_transaction::packed_context_free_data>{});
    f("packed_trx", abieos::member_ptr<&packed_transaction::packed_trx>{});
}

using transaction_variant = std::variant<abieos::checksum256, packed_transaction>;

struct transaction_receipt : transaction_receipt_header {
    transaction_variant trx;
};

template <typename F>
constexpr void for_each_field(transaction_receipt*, F f) {
    for_each_field((transaction_receipt_header*)nullptr, f);
    f("trx", abieos::member_ptr<&transaction_receipt::trx>{});
}

struct block_header {
    abieos::block_timestamp          timestamp;
    abieos::name                     producer;
    uint16_t                         confirmed;
    abieos::checksum256              previous;
    abieos::checksum256              transaction_mroot;
    abieos::checksum256              action_mroot;
    uint32_t                         schedule_version;
    std::optional<producer_schedule> new_producers;
    std::vector<extension>           header_extensions;
};

template <typename F>
constexpr void for_each_field(block_header*, F f) {
    f("timestamp", abieos::member_ptr<&block_header::timestamp>{});
    f("producer", abieos::member_ptr<&block_header::producer>{});
    f("confirmed", abieos::member_ptr<&block_header::confirmed>{});
    f("previous", abieos::member_ptr<&block_header::previous>{});
    f("transaction_mroot", abieos::member_ptr<&block_header::transaction_mroot>{});
    f("action_mroot", abieos::member_ptr<&block_header::action_mroot>{});
    f("schedule_version", abieos::member_ptr<&block_header::schedule_version>{});
    f("new_producers", abieos::member_ptr<&block_header::new_producers>{});
    f("header_extensions", abieos::member_ptr<&block_header::header_extensions>{});
}

struct signed_block_header : block_header {
    abieos::signature producer_signature;
};

template <typename F>
constexpr void for_each_field(signed_block_header*, F f) {
    for_each_field((block_header*)nullptr, f);
    f("producer_signature", abieos::member_ptr<&signed_block_header::producer_signature>{});
}

struct signed_block : signed_block_header {
    std::vector<transaction_receipt> transactions;
    std::vector<extension>           block_extensions;
};

template <typename F>
constexpr void for_each_field(signed_block*, F f) {
    for_each_field((signed_block_header*)nullptr, f);
    f("transactions", abieos::member_ptr<&signed_block::transactions>{});
    f("block_extensions", abieos::member_ptr<&signed_block::block_extensions>{});
}

} // namespace state_history
