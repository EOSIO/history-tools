#pragma once

#ifdef EOSIO_CDT_COMPILATION
#include <abieos.hpp>
#include <eosio/check.hpp>
#else
#include <abieos_exception.hpp>
#endif

namespace state_history {

#ifdef EOSIO_CDT_COMPILATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-noreturn"
[[noreturn]] inline void report_error(const std::string& s) { eosio::check(false, s); }
#pragma clang diagnostic pop
#else
[[noreturn]] inline void report_error(const std::string& s) { throw std::runtime_error(s); }
#endif

struct extension {
    uint16_t            type = {};
    eosio::input_stream data = {};
};

EOSIO_REFLECT(extension, type, data)

struct fill_status_v0 {
    uint32_t            head            = {};
    abieos::checksum256 head_id         = {};
    uint32_t            irreversible    = {};
    abieos::checksum256 irreversible_id = {};
    uint32_t            first           = {};
};

EOSIO_REFLECT(fill_status_v0, head, head_id, irreversible, irreversible_id, first)

using fill_status = std::variant<fill_status_v0>;

inline bool operator==(const fill_status_v0& a, fill_status_v0& b) {
    return std::tie(a.head, a.head_id, a.irreversible, a.irreversible_id, a.first) ==
           std::tie(b.head, b.head_id, b.irreversible, b.irreversible_id, b.first);
}

inline bool operator!=(const fill_status_v0& a, fill_status_v0& b) { return !(a == b); }

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
    report_error("unknown status: " + std::to_string((uint8_t)status));
}

inline transaction_status get_transaction_status(const std::string& s) {
    if (s == "executed")
        return transaction_status::executed;
    if (s == "soft_fail")
        return transaction_status::soft_fail;
    if (s == "hard_fail")
        return transaction_status::hard_fail;
    if (s == "delayed")
        return transaction_status::delayed;
    if (s == "expired")
        return transaction_status::expired;
    report_error("unknown status: " + s);
}

template <typename S>
eosio::result<void> from_bin(transaction_status& obj, S& stream) {
    return stream.read_raw(obj);
}

struct block_position {
    uint32_t            block_num = {};
    abieos::checksum256 block_id  = {};
};

EOSIO_REFLECT(block_position, block_num, block_id)

struct get_status_request_v0 {};

EOSIO_REFLECT_EMPTY(get_status_request_v0)

struct get_blocks_request_v0 {
    uint32_t                    start_block_num        = {};
    uint32_t                    end_block_num          = {};
    uint32_t                    max_messages_in_flight = {};
    std::vector<block_position> have_positions         = {};
    bool                        irreversible_only      = {};
    bool                        fetch_block            = {};
    bool                        fetch_traces           = {};
    bool                        fetch_deltas           = {};
};

EOSIO_REFLECT(
    get_blocks_request_v0, start_block_num, end_block_num, max_messages_in_flight, have_positions, irreversible_only, fetch_block,
    fetch_traces, fetch_deltas)

struct get_blocks_ack_request_v0 {
    uint32_t num_messages = {};
};

EOSIO_REFLECT(get_blocks_ack_request_v0, num_messages)

using request = std::variant<get_status_request_v0, get_blocks_request_v0, get_blocks_ack_request_v0>;

struct get_status_result_v0 {
    block_position head                    = {};
    block_position last_irreversible       = {};
    uint32_t       trace_begin_block       = {};
    uint32_t       trace_end_block         = {};
    uint32_t       chain_state_begin_block = {};
    uint32_t       chain_state_end_block   = {};
};

EOSIO_REFLECT(
    get_status_result_v0, head, last_irreversible, trace_begin_block, trace_end_block, chain_state_begin_block, chain_state_end_block)

struct get_blocks_result_v0 {
    block_position                     head              = {};
    block_position                     last_irreversible = {};
    std::optional<block_position>      this_block        = {};
    std::optional<block_position>      prev_block        = {};
    std::optional<eosio::input_stream> block             = {};
    std::optional<eosio::input_stream> traces            = {};
    std::optional<eosio::input_stream> deltas            = {};
};

EOSIO_REFLECT(get_blocks_result_v0, head, last_irreversible, this_block, prev_block, block, traces, deltas)

using result = std::variant<get_status_result_v0, get_blocks_result_v0>;

struct row {
    bool                present = {};
    eosio::input_stream data    = {};
};

EOSIO_REFLECT(row, present, data)

struct table_delta_v0 {
    std::string      name = {};
    std::vector<row> rows = {};
};

EOSIO_REFLECT(table_delta_v0, name, rows)

using table_delta = std::variant<table_delta_v0>;

struct permission_level {
    abieos::name actor      = {};
    abieos::name permission = {};
};

EOSIO_REFLECT(permission_level, actor, permission)

struct account_auth_sequence {
    abieos::name account  = {};
    uint64_t     sequence = {};
};

EOSIO_REFLECT(account_auth_sequence, account, sequence)

struct account_delta {
    abieos::name account = {};
    int64_t      delta   = {};
};

EOSIO_REFLECT(account_delta, account, delta)

struct action_receipt_v0 {
    abieos::name                       receiver        = {};
    abieos::checksum256                act_digest      = {};
    uint64_t                           global_sequence = {};
    uint64_t                           recv_sequence   = {};
    std::vector<account_auth_sequence> auth_sequence   = {};
    abieos::varuint32                  code_sequence   = {};
    abieos::varuint32                  abi_sequence    = {};
};

EOSIO_REFLECT(action_receipt_v0, receiver, act_digest, global_sequence, recv_sequence, auth_sequence, code_sequence, abi_sequence)

using action_receipt = std::variant<action_receipt_v0>;

struct action {
    abieos::name                  account       = {};
    abieos::name                  name          = {};
    std::vector<permission_level> authorization = {};
    eosio::input_stream           data          = {};
};

EOSIO_REFLECT(action, account, name, authorization, data)

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

EOSIO_REFLECT(
    action_trace_v0, action_ordinal, creator_action_ordinal, receipt, receiver, act, context_free, elapsed, console, account_ram_deltas,
    except, error_code)

using action_trace = std::variant<action_trace_v0>;

struct partial_transaction_v0 {
    abieos::time_point_sec           expiration             = {};
    uint16_t                         ref_block_num          = {};
    uint32_t                         ref_block_prefix       = {};
    abieos::varuint32                max_net_usage_words    = {};
    uint8_t                          max_cpu_usage_ms       = {};
    abieos::varuint32                delay_sec              = {};
    std::vector<extension>           transaction_extensions = {};
    std::vector<abieos::signature>   signatures             = {};
    std::vector<eosio::input_stream> context_free_data      = {};
};

EOSIO_REFLECT(
    partial_transaction_v0, expiration, ref_block_num, ref_block_prefix, max_net_usage_words, max_cpu_usage_ms, delay_sec,
    transaction_extensions, signatures, context_free_data)

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

EOSIO_REFLECT(
    transaction_trace_v0, id, status, cpu_usage_us, net_usage_words, elapsed, net_usage, scheduled, action_traces, account_ram_delta,
    except, error_code, failed_dtrx_trace, partial)

using transaction_trace = std::variant<transaction_trace_v0>;

struct recurse_transaction_trace {
    transaction_trace recurse = {};
};

template <typename S>
eosio::result<void> from_bin(recurse_transaction_trace& obj, S& stream) {
    return from_bin(obj.recurse, stream);
}

struct producer_key {
    abieos::name       producer_name     = {};
    abieos::public_key block_signing_key = {};
};

EOSIO_REFLECT(producer_key, producer_name, block_signing_key)

struct producer_schedule {
    uint32_t                  version   = {};
    std::vector<producer_key> producers = {};
};

EOSIO_REFLECT(producer_schedule, version, producers)

struct transaction_receipt_header {
    transaction_status status          = {};
    uint32_t           cpu_usage_us    = {};
    abieos::varuint32  net_usage_words = {};
};

EOSIO_REFLECT(transaction_receipt_header, status, cpu_usage_us, net_usage_words)

struct packed_transaction {
    std::vector<abieos::signature> signatures               = {};
    uint8_t                        compression              = {};
    eosio::input_stream            packed_context_free_data = {};
    eosio::input_stream            packed_trx               = {};
};

EOSIO_REFLECT(packed_transaction, signatures, compression, packed_context_free_data, packed_trx)

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

EOSIO_REFLECT(
    block_header, timestamp, producer, confirmed, previous, transaction_mroot, action_mroot, schedule_version, new_producers,
    header_extensions)

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

// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

struct transaction_header {
    abieos::time_point_sec expiration          = {};
    uint16_t               ref_block_num       = {};
    uint32_t               ref_block_prefix    = {};
    abieos::varuint32      max_net_usage_words = {};
    uint8_t                max_cpu_usage_ms    = {};
    abieos::varuint32      delay_sec           = {};
};

EOSIO_REFLECT(transaction_header, expiration, ref_block_num, ref_block_prefix, max_net_usage_words, max_cpu_usage_ms, delay_sec)

struct transaction : transaction_header {
    std::vector<action>    context_free_actions   = {};
    std::vector<action>    actions                = {};
    std::vector<extension> transaction_extensions = {};
};

ABIEOS_REFLECT(transaction) {
    ABIEOS_BASE(transaction_header);
    ABIEOS_MEMBER(transaction, context_free_actions);
    ABIEOS_MEMBER(transaction, actions);
    ABIEOS_MEMBER(transaction, transaction_extensions);
}

struct code_id {
    uint8_t             vm_type    = {};
    uint8_t             vm_version = {};
    abieos::checksum256 code_hash  = {};
};

EOSIO_REFLECT(code_id, vm_type, vm_version, code_hash)

struct account_v0 {
    abieos::name            name          = {};
    abieos::block_timestamp creation_date = {};
    eosio::input_stream     abi           = {};
};

EOSIO_REFLECT(account_v0, name, creation_date, abi)

using account = std::variant<account_v0>;

struct account_metadata_v0 {
    abieos::name           name             = {};
    bool                   privileged       = {};
    abieos::time_point     last_code_update = {};
    std::optional<code_id> code             = {};
};

EOSIO_REFLECT(account_metadata_v0, name, privileged, last_code_update, code)

using account_metadata = std::variant<account_metadata_v0>;

struct code_v0 {
    uint8_t             vm_type    = {};
    uint8_t             vm_version = {};
    abieos::checksum256 code_hash  = {};
    eosio::input_stream code       = {};
};

EOSIO_REFLECT(code_v0, vm_type, vm_version, code_hash, code)

using code = std::variant<code_v0>;

struct contract_table_v0 {
    abieos::name code  = {};
    abieos::name scope = {};
    abieos::name table = {};
    abieos::name payer = {};
};

EOSIO_REFLECT(contract_table_v0, code, scope, table, payer)

using contract_table = std::variant<contract_table_v0>;

struct contract_row_v0 {
    abieos::name        code        = {};
    abieos::name        scope       = {};
    abieos::name        table       = {};
    uint64_t            primary_key = {};
    abieos::name        payer       = {};
    eosio::input_stream value       = {};
};

EOSIO_REFLECT(contract_row_v0, code, scope, table, primary_key, payer, value)

using contract_row = std::variant<contract_row_v0>;

struct contract_index64_v0 {
    abieos::name code          = {};
    abieos::name scope         = {};
    abieos::name table         = {};
    uint64_t     primary_key   = {};
    abieos::name payer         = {};
    uint64_t     secondary_key = {};
};

EOSIO_REFLECT(contract_index64_v0, code, scope, table, primary_key, payer, secondary_key)

using contract_index64 = std::variant<contract_index64_v0>;

/*
struct contract_index128_v0 {
    abieos::name code          = {};
    abieos::name scope         = {};
    abieos::name table         = {};
    uint64_t     primary_key   = {};
    abieos::name payer         = {};
    uint128_t    secondary_key = {};
};

using contract_index128 = std::variant<contract_index128_v0>;
*/

struct contract_index256_v0 {
    abieos::name        code          = {};
    abieos::name        scope         = {};
    abieos::name        table         = {};
    uint64_t            primary_key   = {};
    abieos::name        payer         = {};
    abieos::checksum256 secondary_key = {};
};

EOSIO_REFLECT(contract_index256_v0, code, scope, table, primary_key, payer, secondary_key)

using contract_index256 = std::variant<contract_index256_v0>;

struct contract_index_double_v0 {
    abieos::name code          = {};
    abieos::name scope         = {};
    abieos::name table         = {};
    uint64_t     primary_key   = {};
    abieos::name payer         = {};
    double       secondary_key = {};
};

EOSIO_REFLECT(contract_index_double_v0, code, scope, table, primary_key, payer, secondary_key)

using contract_index_double = std::variant<contract_index_double_v0>;

struct contract_index_long_double_v0 {
    abieos::name     code          = {};
    abieos::name     scope         = {};
    abieos::name     table         = {};
    uint64_t         primary_key   = {};
    abieos::name     payer         = {};
    abieos::float128 secondary_key = {};
};

EOSIO_REFLECT(contract_index_long_double_v0, code, scope, table, primary_key, payer, secondary_key)

using contract_index_long_double = std::variant<contract_index_long_double_v0>;

struct key_weight {
    abieos::public_key key    = {};
    uint16_t           weight = {};
};

EOSIO_REFLECT(key_weight, key, weight)

struct block_signing_authority_v0 {
    uint32_t                threshold = {};
    std::vector<key_weight> keys      = {};
};

EOSIO_REFLECT(block_signing_authority_v0, threshold, keys)

using block_signing_authority = std::variant<block_signing_authority_v0>;

struct producer_authority {
    abieos::name            producer_name = {};
    block_signing_authority authority     = {};
};

EOSIO_REFLECT(producer_authority, producer_name, authority)

struct producer_authority_schedule {
    uint32_t                        version   = {};
    std::vector<producer_authority> producers = {};
};

EOSIO_REFLECT(producer_authority_schedule, version, producers)

struct chain_config_v0 {
    uint64_t max_block_net_usage                 = {};
    uint32_t target_block_net_usage_pct          = {};
    uint32_t max_transaction_net_usage           = {};
    uint32_t base_per_transaction_net_usage      = {};
    uint32_t net_usage_leeway                    = {};
    uint32_t context_free_discount_net_usage_num = {};
    uint32_t context_free_discount_net_usage_den = {};
    uint32_t max_block_cpu_usage                 = {};
    uint32_t target_block_cpu_usage_pct          = {};
    uint32_t max_transaction_cpu_usage           = {};
    uint32_t min_transaction_cpu_usage           = {};
    uint32_t max_transaction_lifetime            = {};
    uint32_t deferred_trx_expiration_window      = {};
    uint32_t max_transaction_delay               = {};
    uint32_t max_inline_action_size              = {};
    uint16_t max_inline_action_depth             = {};
    uint16_t max_authority_depth                 = {};
};

EOSIO_REFLECT(
    chain_config_v0, max_block_net_usage, target_block_net_usage_pct, max_transaction_net_usage, base_per_transaction_net_usage,
    net_usage_leeway, context_free_discount_net_usage_num, context_free_discount_net_usage_den, max_block_cpu_usage,
    target_block_cpu_usage_pct, max_transaction_cpu_usage, min_transaction_cpu_usage, max_transaction_lifetime,
    deferred_trx_expiration_window, max_transaction_delay, max_inline_action_size, max_inline_action_depth, max_authority_depth)

using chain_config = std::variant<chain_config_v0>;

struct global_property_v0 {
    std::optional<uint32_t> proposed_schedule_block_num = {};
    producer_schedule       proposed_schedule           = {};
    chain_config            configuration               = {};
};

EOSIO_REFLECT(global_property_v0, proposed_schedule_block_num, proposed_schedule, configuration)

struct global_property_v1 {
    std::optional<uint32_t>     proposed_schedule_block_num = {};
    producer_authority_schedule proposed_schedule           = {};
    chain_config                configuration               = {};
    abieos::checksum256         chain_id                    = {};
};

EOSIO_REFLECT(global_property_v1, proposed_schedule_block_num, proposed_schedule, configuration, chain_id)

using global_property = std::variant<global_property_v0, global_property_v1>;

/*
struct generated_transaction_v0 {
    abieos::name         sender     = {};
    uint128_t            sender_id  = {};
    abieos::name         payer      = {};
    abieos::checksum256  trx_id     = {};
    eosio::input_stream packed_trx = {};
};

using generated_transaction = std::variant<generated_transaction_v0>;
*/

struct activated_protocol_feature_v0 {
    abieos::checksum256 feature_digest       = {};
    uint32_t            activation_block_num = {};
};

using activated_protocol_feature = std::variant<activated_protocol_feature_v0>;

struct protocol_state_v0 {
    std::vector<activated_protocol_feature> activated_protocol_features = {};
};

using protocol_state = std::variant<protocol_state_v0>;

struct permission_level_weight {
    permission_level permission = {};
    uint16_t         weight     = {};
};

struct wait_weight {
    uint32_t wait_sec = {};
    uint16_t weight   = {};
};

struct authority {
    uint32_t                             threshold = {};
    std::vector<key_weight>              keys      = {};
    std::vector<permission_level_weight> accounts  = {};
    std::vector<wait_weight>             waits     = {};
};

struct permission_v0 {
    abieos::name       owner        = {};
    abieos::name       name         = {};
    abieos::name       parent       = {};
    abieos::time_point last_updated = {};
    authority          auth         = {};
};

using permission = std::variant<permission_v0>;

struct permission_link_v0 {
    abieos::name account             = {};
    abieos::name code                = {};
    abieos::name message_type        = {};
    abieos::name required_permission = {};
};

using permission_link = std::variant<permission_link_v0>;

struct resource_limits_v0 {
    abieos::name owner      = {};
    int64_t      net_weight = {};
    int64_t      cpu_weight = {};
    int64_t      ram_bytes  = {};
};

using resource_limits = std::variant<resource_limits_v0>;

struct usage_accumulator_v0 {
    uint32_t last_ordinal = {};
    uint64_t value_ex     = {};
    uint64_t consumed     = {};
};

using usage_accumulator = std::variant<usage_accumulator_v0>;

struct resource_usage_v0 {
    abieos::name      owner     = {};
    usage_accumulator net_usage = {};
    usage_accumulator cpu_usage = {};
    uint64_t          ram_usage = {};
};

using resource_usage = std::variant<resource_usage_v0>;

struct resource_limits_state_v0 {
    usage_accumulator average_block_net_usage = {};
    usage_accumulator average_block_cpu_usage = {};
    uint64_t          total_net_weight        = {};
    uint64_t          total_cpu_weight        = {};
    uint64_t          total_ram_bytes         = {};
    uint64_t          virtual_net_limit       = {};
    uint64_t          virtual_cpu_limit       = {};
};

using resource_limits_state = std::variant<resource_limits_state_v0>;

struct resource_limits_ratio_v0 {
    uint64_t numerator   = {};
    uint64_t denominator = {};
};

using resource_limits_ratio = std::variant<resource_limits_ratio_v0>;

struct elastic_limit_parameters_v0 {
    uint64_t              target         = {};
    uint64_t              max            = {};
    uint32_t              periods        = {};
    uint32_t              max_multiplier = {};
    resource_limits_ratio contract_rate  = {};
    resource_limits_ratio expand_rate    = {};
};

using elastic_limit_parameters = std::variant<elastic_limit_parameters_v0>;

struct resource_limits_config_v0 {
    elastic_limit_parameters cpu_limit_parameters             = {};
    elastic_limit_parameters net_limit_parameters             = {};
    uint32_t                 account_cpu_usage_average_window = {};
    uint32_t                 account_net_usage_average_window = {};
};

using resource_limits_config = std::variant<resource_limits_config_v0>;

struct trx_filter {
    bool                              include     = {};
    std::optional<transaction_status> status      = {};
    std::optional<abieos::name>       receiver    = {};
    std::optional<abieos::name>       act_account = {};
    std::optional<abieos::name>       act_name    = {};
};

inline bool matches(const trx_filter& filter, const transaction_trace_v0& ttrace, const action_trace_v0& atrace) {
    if (filter.status && ttrace.status != *filter.status)
        return false;
    if (filter.receiver && atrace.receiver != *filter.receiver)
        return false;
    if (filter.act_account && atrace.act.account != *filter.act_account)
        return false;
    if (filter.act_name && atrace.act.name != *filter.act_name)
        return false;
    return true;
}

inline bool filter(const std::vector<trx_filter>& filters, const transaction_trace_v0& ttrace, const action_trace_v0& atrace) {
    for (auto& filt : filters) {
        if (matches(filt, ttrace, atrace)) {
            if (filt.include)
                return true;
            else
                return false;
        }
    }
    return false;
}

inline bool filter(const std::vector<trx_filter>& filters, const transaction_trace_v0& ttrace) {
    for (auto& atrace : ttrace.action_traces)
        if (filter(filters, ttrace, std::get<0>(atrace)))
            return true;
    return false;
}

} // namespace state_history
