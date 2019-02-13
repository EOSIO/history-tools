// copyright defined in LICENSE.txt

// todo: remaining tables
// todo: read-only non-contract capabilities in /v1/chain

#pragma once
#include "lib-database.hpp"

struct block_info_request {
    block_select first       = {};
    block_select last        = {};
    uint32_t     max_results = {};

    EOSLIB_SERIALIZE(block_info_request, (first)(last)(max_results))
};

template <typename F>
void for_each_member(block_info_request*, F f) {
    f("first", member_ptr<&block_info_request::first>{});
    f("last", member_ptr<&block_info_request::last>{});
    f("max_results", member_ptr<&block_info_request::max_results>{});
}

// todo: versioning issues
// todo: vector<extendable<...>>
struct block_info_response {
    std::vector<block_info>     blocks = {};
    std::optional<block_select> more   = {};

    EOSLIB_SERIALIZE(block_info_response, (blocks)(more))
};

template <typename F>
void for_each_member(block_info_response*, F f) {
    f("blocks", member_ptr<&block_info_response::blocks>{});
    f("more", member_ptr<&block_info_response::more>{});
}

struct tapos_request {
    block_select ref_block      = {};
    uint32_t     expire_seconds = {};
};

template <typename F>
void for_each_member(tapos_request*, F f) {
    f("ref_block", member_ptr<&tapos_request::ref_block>{});
    f("expire_seconds", member_ptr<&tapos_request::expire_seconds>{});
}

// todo: test pushing a transaction with this result
struct tapos_response {
    uint16_t               ref_block_num    = {};
    uint32_t               ref_block_prefix = {};
    eosio::block_timestamp expiration       = eosio::block_timestamp{};
};

template <typename F>
void for_each_member(tapos_response*, F f) {
    f("ref_block_num", member_ptr<&tapos_response::ref_block_num>{});
    f("ref_block_prefix", member_ptr<&tapos_response::ref_block_prefix>{});
    f("expiration", member_ptr<&tapos_response::expiration>{});
}

struct account_request {
    block_select max_block    = {};
    eosio::name  first        = {};
    eosio::name  last         = {};
    uint32_t     max_results  = {};
    bool         include_abi  = {};
    bool         include_code = {};

    EOSLIB_SERIALIZE(account_request, (max_block)(first)(last)(max_results)(include_abi)(include_code))
};

template <typename F>
void for_each_member(account_request*, F f) {
    f("max_block", member_ptr<&account_request::max_block>{});
    f("first", member_ptr<&account_request::first>{});
    f("last", member_ptr<&account_request::last>{});
    f("max_results", member_ptr<&account_request::max_results>{});
    f("include_abi", member_ptr<&account_request::include_abi>{});
    f("include_code", member_ptr<&account_request::include_code>{});
}

// todo: versioning issues
// todo: vector<extendable<...>>
struct account_response {
    std::vector<account>       accounts = {};
    std::optional<eosio::name> more     = {};

    EOSLIB_SERIALIZE(account_response, (accounts)(more))
};

template <typename F>
void for_each_member(account_response*, F f) {
    f("accounts", member_ptr<&account_response::accounts>{});
    f("more", member_ptr<&account_response::more>{});
}

struct abis_request {
    block_select             max_block = {};
    std::vector<eosio::name> names     = {};

    EOSLIB_SERIALIZE(abis_request, (max_block)(names))
};

template <typename F>
void for_each_member(abis_request*, F f) {
    f("max_block", member_ptr<&abis_request::max_block>{});
    f("names", member_ptr<&abis_request::names>{});
}

struct name_abi {
    eosio::name                    name           = {};
    bool                           account_exists = {};
    eosio::datastream<const char*> abi            = {nullptr, 0};

    EOSLIB_SERIALIZE(name_abi, (name)(account_exists)(abi))
};

template <typename F>
void for_each_member(name_abi*, F f) {
    f("name", member_ptr<&name_abi::name>{});
    f("account_exists", member_ptr<&name_abi::account_exists>{});
    f("abi", member_ptr<&name_abi::abi>{});
}

struct abis_response {
    std::vector<name_abi> abis = {};

    EOSLIB_SERIALIZE(abis_response, (abis))
};

template <typename F>
void for_each_member(abis_response*, F f) {
    f("abis", member_ptr<&abis_response::abis>{});
}
