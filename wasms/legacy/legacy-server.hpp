#pragma once

#include <eosio/database.hpp>

struct get_code_result {
    get_code_result(eosio::account a)
    : account_name(a.name)
    , code_hash(a.code_version)
    , wasm(a.code)
    , abi(a.abi)
    {}
    eosio::name account_name;
    eosio::checksum256 code_hash;
    eosio::shared_memory<eosio::datastream<const char*>> wasm;
    eosio::shared_memory<eosio::datastream<const char*>> abi;
};

/// \exclude
STRUCT_REFLECT(get_code_result) {
    STRUCT_MEMBER(get_code_result, account_name)
    STRUCT_MEMBER(get_code_result, code_hash)
    STRUCT_MEMBER(get_code_result, wasm)
    STRUCT_MEMBER(get_code_result, abi)
}

struct get_abi_result {
    get_abi_result(eosio::account a)
    : account_name(a.name)
    , abi(a.abi)
    {}
    eosio::name account_name;
    eosio::shared_memory<eosio::datastream<const char*>> abi;
};

/// \exclude
STRUCT_REFLECT(get_abi_result) {
    STRUCT_MEMBER(get_abi_result, account_name)
    STRUCT_MEMBER(get_abi_result, abi)
}

/*
struct producer_key {
    eosio::name         producer_name;
    eosio::public_key   block_signing_key;
};

/// \exclude
STRUCT_REFLECT(producer_key) {
    STRUCT_MEMBER(producer_key, producer_name)
    STRUCT_MEMBER(producer_key, block_signing_key)
}

struct get_producer_schedule_result {
    uint32_t version;
    std::vector<producer_key> producers;
};

/// \exclude
STRUCT_REFLECT(get_producer_schedule_result) {
    STRUCT_MEMBER(get_producer_schedule_result, version)
    STRUCT_MEMBER(get_producer_schedule_result, producers)
}
*/
