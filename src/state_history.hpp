// copyright defined in LICENSE.txt

#pragma once
#include "abieos.hpp"

namespace eosio { namespace ship_protocol {
    enum class transaction_status : uint8_t;
}}

namespace state_history {

struct fill_status {
    uint32_t            head            = {};
    abieos::checksum256 head_id         = {};
    uint32_t            irreversible    = {};
    abieos::checksum256 irreversible_id = {};
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
inline bool bin_to_native(transaction_status& status, abieos::bin_to_native_state& state, bool) {
    status = transaction_status(abieos::read_raw<uint8_t>(state.bin));
    return true;
}

inline bool json_to_native(transaction_status&, abieos::json_to_native_state&, abieos::event_type, bool) {
    throw abieos::error("json_to_native: transaction_status unsupported");
}

inline void native_to_bin(const transaction_status& obj, std::vector<char>& bin) { abieos::push_raw(bin, static_cast<uint8_t>(obj)); }
#endif

#if 0
inline bool bin_to_native(recurse_transaction_trace& obj, abieos::bin_to_native_state& state, bool start) {
    return abieos::bin_to_native(obj.recurse, state, start);
}

inline bool json_to_native(recurse_transaction_trace& obj, abieos::json_to_native_state& state, abieos::event_type event, bool start) {
    return abieos::json_to_native(obj.recurse, state, event, start);
}

inline void native_to_bin(const recurse_transaction_trace& obj, std::vector<char>& bin) { abieos::native_to_bin(obj.recurse, bin); }
#endif

#if 0
inline void check_variant(eosio::input_stream& bin, const abieos::abi_type& type, uint32_t expected) {
    using namespace std::literals;
    abieos::varuint32 index;
    from_bin(index, bin);
    if (!type.filled_variant)
        throw std::runtime_error(type.name + " is not a variant"s);
    if (index >= type.fields.size())
        throw std::runtime_error("expected "s + type.fields[expected].name + " got " + std::to_string(index));
    if (index != expected)
        throw std::runtime_error("expected "s + type.fields[expected].name + " got " + type.fields[index].name);
}

inline void check_variant(eosio::input_stream& bin, const abieos::abi_type& type, const char* expected) {
    using namespace std::literals;
    auto index = eosio::read_varuint32(bin);
    if (!type.filled_variant)
        throw std::runtime_error(type.name + " is not a variant"s);
    if (index >= type.fields.size())
        throw std::runtime_error("expected "s + expected + " got " + std::to_string(index));
    if (type.fields[index].name != expected)
        throw std::runtime_error("expected "s + expected + " got " + type.fields[index].name);
}
#endif

struct trx_filter {
    bool                                                    include     = {};
    std::optional<eosio::ship_protocol::transaction_status> status      = {};
    std::optional<abieos::name>                             receiver    = {};
    std::optional<abieos::name>                             act_account = {};
    std::optional<abieos::name>                             act_name    = {};
};
#if 0
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
#endif
} // namespace state_history
