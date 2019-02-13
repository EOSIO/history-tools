// copyright defined in LICENSE.txt

#pragma once
#include "lib-database.hpp"

struct token_transfer_key {
    eosio::name                        receipt_receiver = {};
    eosio::name                        account          = {};
    block_select                       block            = {};
    serial_wrapper<eosio::checksum256> transaction_id   = {};
    uint32_t                           action_index     = {};

    // todo: create a shortcut for defining this
    token_transfer_key& operator++() {
        if (++action_index)
            return *this;
        if (increment(transaction_id.value))
            return *this;
        if (increment(block))
            return *this;
        if (++account.value)
            return *this;
        if (++receipt_receiver.value)
            return *this;
        return *this;
    }

    EOSLIB_SERIALIZE(token_transfer_key, (receipt_receiver)(account)(block)(transaction_id)(action_index))
};

template <typename F>
void for_each_member(token_transfer_key*, F f) {
    f("receipt_receiver", member_ptr<&token_transfer_key::receipt_receiver>{});
    f("account", member_ptr<&token_transfer_key::account>{});
    f("block", member_ptr<&token_transfer_key::block>{});
    f("transaction_id", member_ptr<&token_transfer_key::transaction_id>{});
    f("action_index", member_ptr<&token_transfer_key::action_index>{});
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
void for_each_member(token_transfer*, F f) {
    f("key", member_ptr<&token_transfer::key>{});
    f("from", member_ptr<&token_transfer::from>{});
    f("to", member_ptr<&token_transfer::to>{});
    f("quantity", member_ptr<&token_transfer::quantity>{});
    f("memo", member_ptr<&token_transfer::memo>{});
}

// todo: version
struct token_transfer_request {
    block_select       max_block               = {};
    token_transfer_key first_key               = {};
    token_transfer_key last_key                = {};
    bool               include_notify_incoming = false;
    bool               include_notify_outgoing = false;
    bool               include_nonnotify       = false;
    uint32_t           max_results             = {};
};

template <typename F>
void for_each_member(token_transfer_request*, F f) {
    f("max_block", member_ptr<&token_transfer_request::max_block>{});
    f("first_key", member_ptr<&token_transfer_request::first_key>{});
    f("last_key", member_ptr<&token_transfer_request::last_key>{});
    f("include_notify_incoming", member_ptr<&token_transfer_request::include_notify_incoming>{});
    f("include_notify_outgoing", member_ptr<&token_transfer_request::include_notify_outgoing>{});
    f("include_nonnotify", member_ptr<&token_transfer_request::include_nonnotify>{});
    f("max_results", member_ptr<&token_transfer_request::max_results>{});
}

// todo: version
struct token_transfer_response {
    std::vector<token_transfer>       transfers = {};
    std::optional<token_transfer_key> more      = {};

    EOSLIB_SERIALIZE(token_transfer_response, (transfers)(more))
};

template <typename F>
void for_each_member(token_transfer_response*, F f) {
    f("transfers", member_ptr<&token_transfer_response::transfers>{});
    f("more", member_ptr<&token_transfer_response::more>{});
}

// todo: version
struct balances_for_multiple_accounts_request {
    block_select       max_block     = {};
    eosio::name        code          = {};
    eosio::symbol_code sym           = {};
    eosio::name        first_account = {};
    eosio::name        last_account  = {};
    uint32_t           max_results   = {};
};

template <typename F>
void for_each_member(balances_for_multiple_accounts_request*, F f) {
    f("max_block", member_ptr<&balances_for_multiple_accounts_request::max_block>{});
    f("code", member_ptr<&balances_for_multiple_accounts_request::code>{});
    f("sym", member_ptr<&balances_for_multiple_accounts_request::sym>{});
    f("first_account", member_ptr<&balances_for_multiple_accounts_request::first_account>{});
    f("last_account", member_ptr<&balances_for_multiple_accounts_request::last_account>{});
    f("max_results", member_ptr<&balances_for_multiple_accounts_request::max_results>{});
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
void for_each_member(bfmt_key*, F f) {
    f("sym", member_ptr<&bfmt_key::sym>{});
    f("code", member_ptr<&bfmt_key::code>{});
}

// todo: version
struct balances_for_multiple_tokens_request {
    block_select max_block   = {};
    eosio::name  account     = {};
    bfmt_key     first_key   = {};
    bfmt_key     last_key    = {};
    uint32_t     max_results = {};
};

template <typename F>
void for_each_member(balances_for_multiple_tokens_request*, F f) {
    f("max_block", member_ptr<&balances_for_multiple_tokens_request::max_block>{});
    f("account", member_ptr<&balances_for_multiple_tokens_request::account>{});
    f("first_key", member_ptr<&balances_for_multiple_tokens_request::first_key>{});
    f("last_key", member_ptr<&balances_for_multiple_tokens_request::last_key>{});
    f("max_results", member_ptr<&balances_for_multiple_tokens_request::max_results>{});
}

struct token_balance {
    eosio::name           account = {};
    eosio::extended_asset amount  = {};
};

template <typename F>
void for_each_member(token_balance*, F f) {
    f("account", member_ptr<&token_balance::account>{});
    f("amount", member_ptr<&token_balance::amount>{});
}

// todo: version
struct balances_for_multiple_accounts_response {
    std::vector<token_balance> balances = {};
    std::optional<eosio::name> more     = {};

    EOSLIB_SERIALIZE(balances_for_multiple_accounts_response, (balances)(more))
};

template <typename F>
void for_each_member(balances_for_multiple_accounts_response*, F f) {
    f("balances", member_ptr<&balances_for_multiple_accounts_response::balances>{});
    f("more", member_ptr<&balances_for_multiple_accounts_response::more>{});
}

// todo: version
struct balances_for_multiple_tokens_response {
    std::vector<token_balance> balances = {};
    std::optional<bfmt_key>    more     = {};

    EOSLIB_SERIALIZE(balances_for_multiple_tokens_response, (balances)(more))
};

template <typename F>
void for_each_member(balances_for_multiple_tokens_response*, F f) {
    f("balances", member_ptr<&balances_for_multiple_tokens_response::balances>{});
    f("more", member_ptr<&balances_for_multiple_tokens_response::more>{});
}
