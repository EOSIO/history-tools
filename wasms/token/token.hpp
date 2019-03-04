// copyright defined in LICENSE.txt

#pragma once
#include <eosio/database.hpp>

struct token_transfer_key {
    eosio::name                        receipt_receiver = {};
    eosio::name                        account          = {};
    eosio::block_select                block            = {};
    serial_wrapper<eosio::checksum256> transaction_id   = {};
    uint32_t                           action_index     = {};

    EOSLIB_SERIALIZE(token_transfer_key, (receipt_receiver)(account)(block)(transaction_id)(action_index))
};

STRUCT_REFLECT(token_transfer_key) {
    STRUCT_MEMBER(token_transfer_key, receipt_receiver)
    STRUCT_MEMBER(token_transfer_key, account)
    STRUCT_MEMBER(token_transfer_key, block)
    STRUCT_MEMBER(token_transfer_key, transaction_id)
    STRUCT_MEMBER(token_transfer_key, action_index)
}

struct token_transfer {
    token_transfer_key    key      = {};
    eosio::name           from     = {};
    eosio::name           to       = {};
    eosio::extended_asset quantity = {};
    std::string_view      memo     = {nullptr, 0};

    EOSLIB_SERIALIZE(token_transfer, (key)(from)(to)(quantity)(memo))
};

STRUCT_REFLECT(token_transfer) {
    STRUCT_MEMBER(token_transfer, key)
    STRUCT_MEMBER(token_transfer, from)
    STRUCT_MEMBER(token_transfer, to)
    STRUCT_MEMBER(token_transfer, quantity)
    STRUCT_MEMBER(token_transfer, memo)
}

// todo: version
struct token_transfer_request {
    eosio::block_select max_block               = {};
    token_transfer_key  first_key               = {};
    token_transfer_key  last_key                = {};
    bool                include_notify_incoming = false;
    bool                include_notify_outgoing = false;
    bool                include_nonnotify       = false;
    uint32_t            max_results             = {};
};

STRUCT_REFLECT(token_transfer_request) {
    STRUCT_MEMBER(token_transfer_request, max_block)
    STRUCT_MEMBER(token_transfer_request, first_key)
    STRUCT_MEMBER(token_transfer_request, last_key)
    STRUCT_MEMBER(token_transfer_request, include_notify_incoming)
    STRUCT_MEMBER(token_transfer_request, include_notify_outgoing)
    STRUCT_MEMBER(token_transfer_request, include_nonnotify)
    STRUCT_MEMBER(token_transfer_request, max_results)
}

// todo: version
struct token_transfer_response {
    std::vector<token_transfer>       transfers = {};
    std::optional<token_transfer_key> more      = {};

    EOSLIB_SERIALIZE(token_transfer_response, (transfers)(more))
};

STRUCT_REFLECT(token_transfer_response) {
    STRUCT_MEMBER(token_transfer_response, transfers)
    STRUCT_MEMBER(token_transfer_response, more)
}

// todo: version
struct balances_for_multiple_accounts_request {
    eosio::block_select max_block     = {};
    eosio::name         code          = {};
    eosio::symbol_code  sym           = {};
    eosio::name         first_account = {};
    eosio::name         last_account  = {};
    uint32_t            max_results   = {};
};

STRUCT_REFLECT(balances_for_multiple_accounts_request) {
    STRUCT_MEMBER(balances_for_multiple_accounts_request, max_block)
    STRUCT_MEMBER(balances_for_multiple_accounts_request, code)
    STRUCT_MEMBER(balances_for_multiple_accounts_request, sym)
    STRUCT_MEMBER(balances_for_multiple_accounts_request, first_account)
    STRUCT_MEMBER(balances_for_multiple_accounts_request, last_account)
    STRUCT_MEMBER(balances_for_multiple_accounts_request, max_results)
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

STRUCT_REFLECT(bfmt_key) {
    STRUCT_MEMBER(bfmt_key, sym)
    STRUCT_MEMBER(bfmt_key, code)
}

// todo: version
struct balances_for_multiple_tokens_request {
    eosio::block_select max_block   = {};
    eosio::name         account     = {};
    bfmt_key            first_key   = {};
    bfmt_key            last_key    = {};
    uint32_t            max_results = {};
};

STRUCT_REFLECT(balances_for_multiple_tokens_request) {
    STRUCT_MEMBER(balances_for_multiple_tokens_request, max_block)
    STRUCT_MEMBER(balances_for_multiple_tokens_request, account)
    STRUCT_MEMBER(balances_for_multiple_tokens_request, first_key)
    STRUCT_MEMBER(balances_for_multiple_tokens_request, last_key)
    STRUCT_MEMBER(balances_for_multiple_tokens_request, max_results)
}

struct token_balance {
    eosio::name           account = {};
    eosio::extended_asset amount  = {};
};

STRUCT_REFLECT(token_balance) {
    STRUCT_MEMBER(token_balance, account)
    STRUCT_MEMBER(token_balance, amount)
}

// todo: version
struct balances_for_multiple_accounts_response {
    std::vector<token_balance> balances = {};
    std::optional<eosio::name> more     = {};

    EOSLIB_SERIALIZE(balances_for_multiple_accounts_response, (balances)(more))
};

STRUCT_REFLECT(balances_for_multiple_accounts_response) {
    STRUCT_MEMBER(balances_for_multiple_accounts_response, balances)
    STRUCT_MEMBER(balances_for_multiple_accounts_response, more)
}

// todo: version
struct balances_for_multiple_tokens_response {
    std::vector<token_balance> balances = {};
    std::optional<bfmt_key>    more     = {};

    EOSLIB_SERIALIZE(balances_for_multiple_tokens_response, (balances)(more))
};

STRUCT_REFLECT(balances_for_multiple_tokens_response) {
    STRUCT_MEMBER(balances_for_multiple_tokens_response, balances)
    STRUCT_MEMBER(balances_for_multiple_tokens_response, more)
}

using token_query_request = eosio::tagged_variant<                                //
    eosio::serialize_tag_as_name,                                                 //
    eosio::tagged_type<"transfer"_n, token_transfer_request>,                     //
    eosio::tagged_type<"bal.mult.acc"_n, balances_for_multiple_accounts_request>, //
    eosio::tagged_type<"bal.mult.tok"_n, balances_for_multiple_tokens_request>>;  //

using token_query_response = eosio::tagged_variant<                                //
    eosio::serialize_tag_as_name,                                                  //
    eosio::tagged_type<"transfer"_n, token_transfer_response>,                     //
    eosio::tagged_type<"bal.mult.acc"_n, balances_for_multiple_accounts_response>, //
    eosio::tagged_type<"bal.mult.tok"_n, balances_for_multiple_tokens_response>>;  //
