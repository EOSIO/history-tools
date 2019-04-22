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

template <typename F>
void for_each_member(get_code_result*, F f) {
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

template <typename F>
void for_each_member(get_abi_result*, F f) {
    STRUCT_MEMBER(get_abi_result, account_name)
    STRUCT_MEMBER(get_abi_result, abi)
}
