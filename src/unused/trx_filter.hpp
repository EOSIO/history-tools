#pragma once

#include <eosio/history-tools/state_history.hpp>

namespace state_history {

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
