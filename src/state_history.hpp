// copyright defined in LICENSE.txt

#pragma once
#include "abieos_exception.hpp"

namespace state_history {

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

template <typename F>
constexpr void for_each_field(block_position*, F f) {
    f("block_num", abieos::member_ptr<&block_position::block_num>{});
    f("block_id", abieos::member_ptr<&block_position::block_id>{});
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
    bool                 present = {};
    abieos::input_buffer data    = {};
};

template <typename F>
constexpr void for_each_field(row*, F f) {
    f("present", abieos::member_ptr<&row::present>{});
    f("data", abieos::member_ptr<&row::data>{});
}

struct table_delta_v0 {
    std::string      name = {};
    std::vector<row> rows = {};
};

template <typename F>
constexpr void for_each_field(table_delta_v0*, F f) {
    f("name", abieos::member_ptr<&table_delta_v0::name>{});
    f("rows", abieos::member_ptr<&table_delta_v0::rows>{});
}

struct permission_level {
    abieos::name actor      = {};
    abieos::name permission = {};
};

template <typename F>
constexpr void for_each_field(permission_level*, F f) {
    f("actor", abieos::member_ptr<&permission_level::actor>{});
    f("permission", abieos::member_ptr<&permission_level::permission>{});
}

struct account_auth_sequence {
    abieos::name account  = {};
    uint64_t     sequence = {};
};

template <typename F>
constexpr void for_each_field(account_auth_sequence*, F f) {
    f("account", abieos::member_ptr<&account_auth_sequence::account>{});
    f("sequence", abieos::member_ptr<&account_auth_sequence::sequence>{});
}

struct account_delta {
    abieos::name account = {};
    int64_t      delta   = {};
};

template <typename F>
constexpr void for_each_field(account_delta*, F f) {
    f("account", abieos::member_ptr<&account_delta::account>{});
    f("delta", abieos::member_ptr<&account_delta::delta>{});
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

template <typename F>
constexpr void for_each_field(action_receipt_v0*, F f) {
    f("receiver", abieos::member_ptr<&action_receipt_v0::receiver>{});
    f("act_digest", abieos::member_ptr<&action_receipt_v0::act_digest>{});
    f("global_sequence", abieos::member_ptr<&action_receipt_v0::global_sequence>{});
    f("recv_sequence", abieos::member_ptr<&action_receipt_v0::recv_sequence>{});
    f("auth_sequence", abieos::member_ptr<&action_receipt_v0::auth_sequence>{});
    f("code_sequence", abieos::member_ptr<&action_receipt_v0::code_sequence>{});
    f("abi_sequence", abieos::member_ptr<&action_receipt_v0::abi_sequence>{});
}

using action_receipt = std::variant<action_receipt_v0>;

struct action {
    abieos::name                  account       = {};
    abieos::name                  name          = {};
    std::vector<permission_level> authorization = {};
    abieos::input_buffer          data          = {};
};

template <typename F>
constexpr void for_each_field(action*, F f) {
    f("account", abieos::member_ptr<&action::account>{});
    f("name", abieos::member_ptr<&action::name>{});
    f("authorization", abieos::member_ptr<&action::authorization>{});
    f("data", abieos::member_ptr<&action::data>{});
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
};

template <typename F>
constexpr void for_each_field(action_trace_v0*, F f) {
    f("action_ordinal", abieos::member_ptr<&action_trace_v0::action_ordinal>{});
    f("creator_action_ordinal", abieos::member_ptr<&action_trace_v0::creator_action_ordinal>{});
    f("receipt", abieos::member_ptr<&action_trace_v0::receipt>{});
    f("receiver", abieos::member_ptr<&action_trace_v0::receiver>{});
    f("act", abieos::member_ptr<&action_trace_v0::act>{});
    f("context_free", abieos::member_ptr<&action_trace_v0::context_free>{});
    f("elapsed", abieos::member_ptr<&action_trace_v0::elapsed>{});
    f("console", abieos::member_ptr<&action_trace_v0::console>{});
    f("account_ram_deltas", abieos::member_ptr<&action_trace_v0::account_ram_deltas>{});
    f("except", abieos::member_ptr<&action_trace_v0::except>{});
}

using action_trace = std::variant<action_trace_v0>;

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
    std::vector<recurse_transaction_trace> failed_dtrx_trace = {};
};

template <typename F>
constexpr void for_each_field(transaction_trace_v0*, F f) {
    f("id", abieos::member_ptr<&transaction_trace_v0::id>{});
    f("status", abieos::member_ptr<&transaction_trace_v0::status>{});
    f("cpu_usage_us", abieos::member_ptr<&transaction_trace_v0::cpu_usage_us>{});
    f("net_usage_words", abieos::member_ptr<&transaction_trace_v0::net_usage_words>{});
    f("elapsed", abieos::member_ptr<&transaction_trace_v0::elapsed>{});
    f("net_usage", abieos::member_ptr<&transaction_trace_v0::net_usage>{});
    f("scheduled", abieos::member_ptr<&transaction_trace_v0::scheduled>{});
    f("action_traces", abieos::member_ptr<&transaction_trace_v0::action_traces>{});
    f("account_ram_delta", abieos::member_ptr<&transaction_trace_v0::account_ram_delta>{});
    f("except", abieos::member_ptr<&transaction_trace_v0::except>{});
    f("failed_dtrx_trace", abieos::member_ptr<&transaction_trace_v0::failed_dtrx_trace>{});
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

template <typename F>
constexpr void for_each_field(producer_key*, F f) {
    f("producer_name", abieos::member_ptr<&producer_key::producer_name>{});
    f("block_signing_key", abieos::member_ptr<&producer_key::block_signing_key>{});
}

struct extension {
    uint16_t             type = {};
    abieos::input_buffer data = {};
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
    transaction_status status          = {};
    uint32_t           cpu_usage_us    = {};
    abieos::varuint32  net_usage_words = {};
};

template <typename F>
constexpr void for_each_field(transaction_receipt_header*, F f) {
    f("status", abieos::member_ptr<&transaction_receipt_header::status>{});
    f("cpu_usage_us", abieos::member_ptr<&transaction_receipt_header::cpu_usage_us>{});
    f("net_usage_words", abieos::member_ptr<&transaction_receipt_header::net_usage_words>{});
}

struct packed_transaction {
    std::vector<abieos::signature> signatures               = {};
    uint8_t                        compression              = {};
    abieos::input_buffer           packed_context_free_data = {};
    abieos::input_buffer           packed_trx               = {};
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
    transaction_variant trx = {};
};

template <typename F>
constexpr void for_each_field(transaction_receipt*, F f) {
    for_each_field((transaction_receipt_header*)nullptr, f);
    f("trx", abieos::member_ptr<&transaction_receipt::trx>{});
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
    abieos::signature producer_signature = {};
};

template <typename F>
constexpr void for_each_field(signed_block_header*, F f) {
    for_each_field((block_header*)nullptr, f);
    f("producer_signature", abieos::member_ptr<&signed_block_header::producer_signature>{});
}

struct signed_block : signed_block_header {
    std::vector<transaction_receipt> transactions     = {};
    std::vector<extension>           block_extensions = {};
};

template <typename F>
constexpr void for_each_field(signed_block*, F f) {
    for_each_field((signed_block_header*)nullptr, f);
    f("transactions", abieos::member_ptr<&signed_block::transactions>{});
    f("block_extensions", abieos::member_ptr<&signed_block::block_extensions>{});
}

} // namespace state_history
