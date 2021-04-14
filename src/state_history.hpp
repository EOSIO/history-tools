// copyright defined in LICENSE.txt

#pragma once
#include <eosio/ship_protocol.hpp>
#include <eosio/abi.hpp>
namespace eosio { namespace ship_protocol {
    enum class transaction_status : uint8_t;
}}

namespace state_history {

struct fill_status {
    uint32_t            head            = {};
    eosio::checksum256  head_id         = {};
    uint32_t            irreversible    = {};
    eosio::checksum256  irreversible_id = {};
    uint32_t            first           = {};
};

EOSIO_REFLECT(fill_status,
    head,
    head_id,
    irreversible,
    irreversible_id,
    first);

inline bool operator==(const fill_status& a, fill_status& b) {
    return std::tie(a.head, a.head_id, a.irreversible, a.irreversible_id, a.first) ==
           std::tie(b.head, b.head_id, b.irreversible, b.irreversible_id, b.first);
}

inline bool operator!=(const fill_status& a, fill_status& b) { return !(a == b); }

#if 0
inline bool bin_to_native(transaction_status& status, eosio::bin_to_native_state& state, bool) {
    status = transaction_status(eosio::read_raw<uint8_t>(state.bin));
    return true;
}

inline bool json_to_native(transaction_status&, eosio::json_to_native_state&, eosio::event_type, bool) {
    throw eosio::error("json_to_native: transaction_status unsupported");
}

inline void native_to_bin(const transaction_status& obj, std::vector<char>& bin) { eosio::push_raw(bin, static_cast<uint8_t>(obj)); }
#endif

#if 0
inline bool bin_to_native(recurse_transaction_trace& obj, eosio::bin_to_native_state& state, bool start) {
    return eosio::bin_to_native(obj.recurse, state, start);
}

inline bool json_to_native(recurse_transaction_trace& obj, eosio::json_to_native_state& state, eosio::event_type event, bool start) {
    return eosio::json_to_native(obj.recurse, state, event, start);
}

inline void native_to_bin(const recurse_transaction_trace& obj, std::vector<char>& bin) { eosio::native_to_bin(obj.recurse, bin); }
#endif

inline void check_variant(eosio::input_stream& bin, const eosio::abi_type& type, uint32_t expected) {
    using namespace std::literals;
    uint32_t index;
    varuint32_from_bin(index, bin);
    if (!type.as_variant())
        throw std::runtime_error(type.name + " is not a variant"s);
    if (index >= type.as_variant()->size())
        throw std::runtime_error("expected "s + type.as_variant()->at(expected).name + " got " + std::to_string(index));
    if (index != expected)
        throw std::runtime_error("expected "s + type.as_variant()->at(expected).name + " got " + type.as_variant()->at(index).name);
}

inline void check_variant(eosio::input_stream& bin, const eosio::abi_type& type, const char* expected) {
    using namespace std::literals;
    uint32_t index;
    eosio::varuint32_from_bin(index, bin);
    if (!type.as_variant())
        throw std::runtime_error(type.name + " is not a variant"s);
    if (index >= type.as_variant()->size())
        throw std::runtime_error("expected "s + expected + " got " + std::to_string(index));
    if (type.as_variant()->at(index).name != expected)
        throw std::runtime_error("expected "s + expected + " got " + type.as_variant()->at(index).name);
}

struct trx_filter {
    bool                                                    include     = {};
    std::optional<eosio::ship_protocol::transaction_status> status      = {};
    std::optional<eosio::name>                             receiver    = {};
    std::optional<eosio::name>                             act_account = {};
    std::optional<eosio::name>                             act_name    = {};
};

inline bool matches(const trx_filter& filter, const eosio::ship_protocol::transaction_status status, const eosio::ship_protocol::action_trace& atrace) {
    if (filter.status && status != *filter.status)
        return false;
    if (filter.receiver && std::visit([](auto&& arg){return arg.receiver;}, atrace) != *filter.receiver)
        return false;
    if (filter.act_account && std::visit([](auto&& arg){return arg.act.account;}, atrace) != *filter.act_account)
        return false;
    if (filter.act_name && std::visit([](auto&& arg){return arg.act.name;}, atrace) != *filter.act_name)
        return false;
    return true;
}

inline bool filter(const std::vector<trx_filter>& filters, const eosio::ship_protocol::transaction_status& status, const eosio::ship_protocol::action_trace& atrace) {
    for (auto& filt : filters) {
        if (matches(filt, status, atrace)) {
            if (filt.include)
                return true;
            else
                return false;
        }
    }
    return false;
}

inline bool filter(const std::vector<trx_filter>& filters, const eosio::ship_protocol::transaction_trace& trace) {
    auto status = std::visit([](auto&& ttrace) { return ttrace.status; }, trace);
    auto action_traces = std::visit([](auto&& ttrace) { return ttrace.action_traces; }, trace);

    for (auto& atrace : action_traces)
        if (filter(filters, status, atrace))
            return true;
    return false;
}

} // namespace state_history
