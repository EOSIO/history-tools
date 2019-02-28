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

inline std::string_view schema_type_name(block_info_request*) { return "block_info_request"; }

STRUCT_REFLECT(block_info_request) {
    STRUCT_MEMBER(block_info_request, first)
    STRUCT_MEMBER(block_info_request, last)
    STRUCT_MEMBER(block_info_request, max_results)
}

// todo: versioning issues
// todo: vector<extendable<...>>
struct block_info_response {
    std::vector<block_info>     blocks = {};
    std::optional<block_select> more   = {};

    EOSLIB_SERIALIZE(block_info_response, (blocks)(more))
};

inline std::string_view schema_type_name(block_info_response*) { return "block_info_response"; }

STRUCT_REFLECT(block_info_response) {
    STRUCT_MEMBER(block_info_response, blocks)
    STRUCT_MEMBER(block_info_response, more)
}

struct tapos_request {
    block_select ref_block      = {};
    uint32_t     expire_seconds = {};
};

inline std::string_view schema_type_name(tapos_request*) { return "tapos_request"; }

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

inline std::string_view schema_type_name(tapos_response*) { return "tapos_response"; }

STRUCT_REFLECT(tapos_response) {
    STRUCT_MEMBER(tapos_response, ref_block_num)
    STRUCT_MEMBER(tapos_response, ref_block_prefix)
    STRUCT_MEMBER(tapos_response, expiration)
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

inline std::string_view schema_type_name(account_request*) { return "account_request"; }

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
    std::vector<account>       accounts = {};
    std::optional<eosio::name> more     = {};

    EOSLIB_SERIALIZE(account_response, (accounts)(more))
};

inline std::string_view schema_type_name(account_response*) { return "account_response"; }

STRUCT_REFLECT(account_response) {
    STRUCT_MEMBER(account_response, accounts)
    STRUCT_MEMBER(account_response, more)
}

struct abis_request {
    block_select             max_block = {};
    std::vector<eosio::name> names     = {};

    EOSLIB_SERIALIZE(abis_request, (max_block)(names))
};

inline std::string_view schema_type_name(abis_request*) { return "abis_request"; }

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

inline std::string_view schema_type_name(name_abi*) { return "name_abi"; }

STRUCT_REFLECT(name_abi) {
    STRUCT_MEMBER(name_abi, name)
    STRUCT_MEMBER(name_abi, account_exists)
    STRUCT_MEMBER(name_abi, abi)
}

struct abis_response {
    std::vector<name_abi> abis = {};

    EOSLIB_SERIALIZE(abis_response, (abis))
};

inline std::string_view schema_type_name(abis_response*) { return "abis_response"; }

STRUCT_REFLECT(abis_response) { //
    STRUCT_MEMBER(abis_response, abis)
}
