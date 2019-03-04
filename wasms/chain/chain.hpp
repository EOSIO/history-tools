// copyright defined in LICENSE.txt

// todo: remaining tables
// todo: read-only non-contract capabilities in /v1/chain

#pragma once
#include <eosio/database.hpp>

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
    eosio::block_select max_block    = {};
    eosio::name         first        = {};
    eosio::name         last         = {};
    uint32_t            max_results  = {};
    bool                include_abi  = {};
    bool                include_code = {};

    EOSLIB_SERIALIZE(account_request, (max_block)(first)(last)(max_results)(include_abi)(include_code))
};

STRUCT_REFLECT(account_request) {
    STRUCT_MEMBER(account_request, max_block)
    STRUCT_MEMBER(account_request, first)
    STRUCT_MEMBER(account_request, last)
    STRUCT_MEMBER(account_request, max_results)
    STRUCT_MEMBER(account_request, include_abi)
    STRUCT_MEMBER(account_request, include_code)
}

// todo: versioning issues
// todo: vector<extendable<...>>
struct account_response {
    std::vector<eosio::account> accounts = {};
    std::optional<eosio::name>  more     = {};

    EOSLIB_SERIALIZE(account_response, (accounts)(more))
};

STRUCT_REFLECT(account_response) {
    STRUCT_MEMBER(account_response, accounts)
    STRUCT_MEMBER(account_response, more)
}

struct abis_request {
    eosio::block_select      max_block = {};
    std::vector<eosio::name> names     = {};

    EOSLIB_SERIALIZE(abis_request, (max_block)(names))
};

STRUCT_REFLECT(abis_request) {
    STRUCT_MEMBER(abis_request, max_block)
    STRUCT_MEMBER(abis_request, names)
}

struct name_abi {
    eosio::name                    name           = {};
    bool                           account_exists = {};
    eosio::datastream<const char*> abi            = {nullptr, 0};

    EOSLIB_SERIALIZE(name_abi, (name)(account_exists)(abi))
};

STRUCT_REFLECT(name_abi) {
    STRUCT_MEMBER(name_abi, name)
    STRUCT_MEMBER(name_abi, account_exists)
    STRUCT_MEMBER(name_abi, abi)
}

struct abis_response {
    std::vector<name_abi> abis = {};

    EOSLIB_SERIALIZE(abis_response, (abis))
};

STRUCT_REFLECT(abis_response) { //
    STRUCT_MEMBER(abis_response, abis)
}

using chain_request = eosio::tagged_variant<                //
    eosio::serialize_tag_as_name,                           //
    eosio::tagged_type<"block.info"_n, block_info_request>, //
    eosio::tagged_type<"tapos"_n, tapos_request>,           //
    eosio::tagged_type<"account"_n, account_request>,       //
    eosio::tagged_type<"abis"_n, abis_request>>;            //

using chain_response = eosio::tagged_variant<                //
    eosio::serialize_tag_as_name,                            //
    eosio::tagged_type<"block.info"_n, block_info_response>, //
    eosio::tagged_type<"tapos"_n, tapos_response>,           //
    eosio::tagged_type<"account"_n, account_response>,       //
    eosio::tagged_type<"abis"_n, abis_response>>;            //
