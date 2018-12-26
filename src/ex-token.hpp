// copyright defined in LICENSE.txt

#pragma once
#include "lib-placeholders.hpp"

// todo: block_index: head, irreversible options
struct token_transfer_key {
    eosio::name                        receipt_receiver = {};
    eosio::name                        account          = {};
    uint32_t                           block_index      = {};
    serial_wrapper<eosio::checksum256> transaction_id   = {};
    uint32_t                           action_index     = {};

    // todo: create a shortcut for defining this
    token_transfer_key& operator++() {
        if (++action_index)
            return *this;
        if (!increment(transaction_id.value))
            return *this;
        if (++block_index)
            return *this;
        if (++account.value)
            return *this;
        if (++receipt_receiver.value)
            return *this;
        return *this;
    }

    EOSLIB_SERIALIZE(token_transfer_key, (receipt_receiver)(account)(block_index)(transaction_id)(action_index))
};

template <typename F>
void for_each_member(token_transfer_key& obj, F f) {
    f("receipt_receiver", obj.receipt_receiver);
    f("account", obj.account);
    f("block_index", obj.block_index);
    f("transaction_id", obj.transaction_id);
    f("action_index", obj.action_index);
}

struct token_transfer {
    token_transfer_key    key      = {};
    eosio::name           from     = {};
    eosio::name           to       = {};
    eosio::extended_asset quantity = {};
    std::string_view      memo     = {nullptr, 0};

    EOSLIB_SERIALIZE(token_transfer, (key)(from)(to)(quantity)(memo))
};

template <typename F>
void for_each_member(token_transfer& obj, F f) {
    f("key", obj.key);
    f("from", obj.from);
    f("to", obj.to);
    f("quantity", obj.quantity);
    f("memo", obj.memo);
}

// todo: version
struct token_transfer_request {
    eosio::name        request                 = "transfer"_n; // todo: remove
    block_select       max_block               = {};
    token_transfer_key first_key               = {};
    token_transfer_key last_key                = {};
    bool               include_notify_incoming = false;
    bool               include_notify_outgoing = false;
    bool               include_nonnotify       = false;
    uint32_t           max_results             = {};
};

template <typename F>
void for_each_member(token_transfer_request& obj, F f) {
    f("request", obj.request);
    f("max_block", obj.max_block);
    f("first_key", obj.first_key);
    f("last_key", obj.last_key);
    f("include_notify_incoming", obj.include_notify_incoming);
    f("include_notify_outgoing", obj.include_notify_outgoing);
    f("include_nonnotify", obj.include_nonnotify);
    f("max_results", obj.max_results);
}

// todo: version
struct token_transfer_response {
    std::vector<token_transfer>       transfers = {};
    std::optional<token_transfer_key> more      = {};

    EOSLIB_SERIALIZE(token_transfer_response, (transfers)(more))
};

template <typename F>
void for_each_member(token_transfer_response& obj, F f) {
    f("transfers", obj.transfers);
    f("more", obj.more);
}

// todo: version
struct balances_for_multiple_accounts_request {
    eosio::name        request       = "bal.mult.acc"_n; // todo: remove
    block_select       max_block     = {};
    eosio::name        code          = {};
    eosio::symbol_code sym           = {};
    eosio::name        first_account = {};
    eosio::name        last_account  = {};
    uint32_t           max_results   = {};
};

template <typename F>
void for_each_member(balances_for_multiple_accounts_request& obj, F f) {
    f("request", obj.request);
    f("max_block", obj.max_block);
    f("code", obj.code);
    f("sym", obj.sym);
    f("first_account", obj.first_account);
    f("last_account", obj.last_account);
    f("max_results", obj.max_results);
}

struct bfmt_key {
    eosio::symbol_code sym  = {};
    eosio::name        code = {};

    bfmt_key& operator++() {
        code = eosio::name{code.value + 1};
        if (!code.value)
            sym = eosio::symbol_code{sym.raw() + 1};
        return *this;
    }
};

template <typename F>
void for_each_member(bfmt_key& obj, F f) {
    f("sym", obj.sym);
    f("code", obj.code);
}

// todo: version
struct balances_for_multiple_tokens_request {
    eosio::name  request     = "bal.mult.tok"_n; // todo: remove
    block_select max_block   = {};
    eosio::name  account     = {};
    bfmt_key     first_key   = {};
    bfmt_key     last_key    = {};
    uint32_t     max_results = {};
};

template <typename F>
void for_each_member(balances_for_multiple_tokens_request& obj, F f) {
    f("max_block", obj.max_block);
    f("account", obj.account);
    f("first_key", obj.first_key);
    f("last_key", obj.last_key);
    f("max_results", obj.max_results);
}

struct token_balance {
    eosio::name           account = {};
    eosio::extended_asset amount  = {};
};

template <typename F>
void for_each_member(token_balance& obj, F f) {
    f("account", obj.account);
    f("amount", obj.amount);
}

// todo: version
struct balances_for_multiple_accounts_response {
    std::vector<token_balance> balances = {};
    std::optional<eosio::name> more     = {};

    EOSLIB_SERIALIZE(balances_for_multiple_accounts_response, (balances)(more))
};

template <typename F>
void for_each_member(balances_for_multiple_accounts_response& obj, F f) {
    f("balances", obj.balances);
    f("more", obj.more);
}

// todo: version
struct balances_for_multiple_tokens_response {
    std::vector<token_balance> balances = {};
    std::optional<bfmt_key>    more     = {};

    EOSLIB_SERIALIZE(balances_for_multiple_tokens_response, (balances)(more))
};

template <typename F>
void for_each_member(balances_for_multiple_tokens_response& obj, F f) {
    f("balances", obj.balances);
    f("more", obj.more);
}
