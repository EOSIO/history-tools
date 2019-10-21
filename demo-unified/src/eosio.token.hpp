#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/unified_lib.hpp>
#include <string>

class token : public eosio::contract {
  public:
    using contract::contract;

    void create(const eosio::name& issuer, const eosio::asset& maximum_supply);

    void issue(const eosio::name& to, const eosio::asset& quantity, const std::string& memo);

    void retire(const eosio::asset& quantity, const std::string& memo);

    void transfer(const eosio::name& from, const eosio::name& to, const eosio::asset& quantity, const std::string& memo);

    void open(const eosio::name& owner, const eosio::symbol& symbol, const eosio::name& ram_payer);

    void close(const eosio::name& owner, const eosio::symbol& symbol);

    struct account {
        eosio::asset balance;

        uint64_t primary_key() const { return balance.symbol.code().raw(); }
    };

    struct currency_stats {
        eosio::asset supply;
        eosio::asset max_supply;
        eosio::name  issuer;

        uint64_t primary_key() const { return supply.symbol.code().raw(); }
    };

    typedef eosio::multi_index<"accounts"_n, account>    accounts;
    typedef eosio::multi_index<"stat"_n, currency_stats> stats;

    void sub_balance(const eosio::name& owner, const eosio::asset& value);
    void add_balance(const eosio::name& owner, const eosio::asset& value, const eosio::name& ram_payer);
};

CONTRACT_ACTIONS(token, create, issue, retire, transfer, open, close)
