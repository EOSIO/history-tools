// copyright defined in LICENSE.txt

// todo: remaining tables
// todo: read-only non-contract capabilities in /v1/chain

#pragma once
#include <eosio/block_select.hpp>

struct block_info_request {
    eosio::block_select first       = {};
    eosio::block_select last        = {};
    uint32_t            max_results = {};

    EOSLIB_SERIALIZE(block_info_request, (first)(last)(max_results))
};

STRUCT_REFLECT(block_info_request) {
    STRUCT_MEMBER(block_info_request, first)
    STRUCT_MEMBER(block_info_request, last)
    STRUCT_MEMBER(block_info_request, max_results)
}

// todo: versioning issues
// todo: vector<extendable<...>>
struct block_info_response {
    std::vector<eosio::block_info>     blocks = {};
    std::optional<eosio::block_select> more   = {};

    EOSLIB_SERIALIZE(block_info_response, (blocks)(more))
};

STRUCT_REFLECT(block_info_response) {
    STRUCT_MEMBER(block_info_response, blocks)
    STRUCT_MEMBER(block_info_response, more)
}

struct tapos_request {
    eosio::block_select ref_block      = {};
    uint32_t            expire_seconds = {};
};

STRUCT_REFLECT(tapos_request) {
    STRUCT_MEMBER(tapos_request, ref_block)
    STRUCT_MEMBER(tapos_request, expire_seconds)
}

// todo: test pushing a transaction with this result
struct tapos_response {
    uint16_t               ref_block_num    = {};
    uint32_t               ref_block_prefix = {};
    eosio::block_timestamp expiration       = eosio::block_timestamp{};
};

STRUCT_REFLECT(tapos_response) {
    STRUCT_MEMBER(tapos_response, ref_block_num)
    STRUCT_MEMBER(tapos_response, ref_block_prefix)
    STRUCT_MEMBER(tapos_response, expiration)
}

struct account_request {
    eosio::block_select snapshot_block = {};
    eosio::name         first          = {};
    eosio::name         last           = {};
    uint32_t            max_results    = {};
    bool                include_abi    = {};

    EOSLIB_SERIALIZE(account_request, (snapshot_block)(first)(last)(max_results)(include_abi))
};

STRUCT_REFLECT(account_request) {
    STRUCT_MEMBER(account_request, snapshot_block)
    STRUCT_MEMBER(account_request, first)
    STRUCT_MEMBER(account_request, last)
    STRUCT_MEMBER(account_request, max_results)
    STRUCT_MEMBER(account_request, include_abi)
}

// todo: versioning issues
// todo: vector<extendable<...>>
// todo: switch from account_metadata_joined to custom type
struct account_response {
    std::vector<eosio::account_metadata_joined> accounts = {};
    std::optional<eosio::name>                  more     = {};

    EOSLIB_SERIALIZE(account_response, (accounts)(more))
};

STRUCT_REFLECT(account_response) {
    STRUCT_MEMBER(account_response, accounts)
    STRUCT_MEMBER(account_response, more)
}

struct abi_request {
    eosio::block_select      snapshot_block = {};
    std::vector<eosio::name> names          = {};

    EOSLIB_SERIALIZE(abi_request, (snapshot_block)(names))
};

STRUCT_REFLECT(abi_request) {
    STRUCT_MEMBER(abi_request, snapshot_block)
    STRUCT_MEMBER(abi_request, names)
}

struct name_abi {
    eosio::name                                          name           = {};
    bool                                                 account_exists = {};
    eosio::shared_memory<eosio::datastream<const char*>> abi            = {};

    EOSLIB_SERIALIZE(name_abi, (name)(account_exists)(abi))
};

STRUCT_REFLECT(name_abi) {
    STRUCT_MEMBER(name_abi, name)
    STRUCT_MEMBER(name_abi, account_exists)
    STRUCT_MEMBER(name_abi, abi)
}

struct abi_response {
    std::vector<name_abi> abis = {};

    EOSLIB_SERIALIZE(abi_response, (abis))
};

STRUCT_REFLECT(abi_response) { //
    STRUCT_MEMBER(abi_response, abis)
}

struct code_request {
    eosio::block_select      snapshot_block = {};
    std::vector<eosio::name> names          = {};

    EOSLIB_SERIALIZE(code_request, (snapshot_block)(names))
};

STRUCT_REFLECT(code_request) {
    STRUCT_MEMBER(code_request, snapshot_block)
    STRUCT_MEMBER(code_request, names)
}

struct name_code {
    eosio::name                                          name           = {};
    bool                                                 account_exists = {};
    eosio::shared_memory<eosio::datastream<const char*>> code           = {};

    EOSLIB_SERIALIZE(name_code, (name)(account_exists)(code))
};

STRUCT_REFLECT(name_code) {
    STRUCT_MEMBER(name_code, name)
    STRUCT_MEMBER(name_code, account_exists)
    STRUCT_MEMBER(name_code, code)
}

struct code_response {
    std::vector<name_code> code = {};

    EOSLIB_SERIALIZE(code_response, (code))
};

STRUCT_REFLECT(code_response) { //
    STRUCT_MEMBER(code_response, code)
}

using chain_query_request = eosio::tagged_variant<          //
    eosio::serialize_tag_as_name,                           //
    eosio::tagged_type<"block.info"_n, block_info_request>, //
    eosio::tagged_type<"tapos"_n, tapos_request>,           //
    eosio::tagged_type<"account"_n, account_request>,       //
    eosio::tagged_type<"abi"_n, abi_request>,               //
    eosio::tagged_type<"code"_n, code_request>>;            //

using chain_query_response = eosio::tagged_variant<          //
    eosio::serialize_tag_as_name,                            //
    eosio::tagged_type<"block.info"_n, block_info_response>, //
    eosio::tagged_type<"tapos"_n, tapos_response>,           //
    eosio::tagged_type<"account"_n, account_response>,       //
    eosio::tagged_type<"abi"_n, abi_response>,               //
    eosio::tagged_type<"code"_n, code_response>>;            //
