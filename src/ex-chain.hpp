// copyright defined in LICENSE.txt

#pragma once
#include "lib-database.hpp"

struct block_info_request {
    uint32_t first       = {}; // todo: block_select
    uint32_t last        = {}; // todo: block_select
    uint32_t max_results = {};

    EOSLIB_SERIALIZE(block_info_request, (first)(last)(max_results))
};

template <typename F>
void for_each_member(block_info_request& obj, F f) {
    f("first", obj.first);
    f("last", obj.last);
    f("max_results", obj.max_results);
}

// todo: versioning issues
// todo: vector<extendable<...>>
struct block_info_response {
    std::vector<block_info> blocks = {};
    std::optional<uint32_t> more   = {};

    EOSLIB_SERIALIZE(block_info_response, (blocks)(more))
};

template <typename F>
void for_each_member(block_info_response& obj, F f) {
    f("blocks", obj.blocks);
    f("more", obj.more);
}

struct tapos_request {
    uint32_t ref_block      = {}; // todo: block_select
    uint32_t expire_seconds = {};
};

template <typename F>
void for_each_member(tapos_request& obj, F f) {
    f("ref_block", obj.ref_block);
    f("expire_seconds", obj.expire_seconds);
}

// todo: test pushing a transaction with this result
struct tapos_response {
    uint16_t               ref_block_num    = {};
    uint32_t               ref_block_prefix = {};
    eosio::block_timestamp expiration       = eosio::block_timestamp{};
};

template <typename F>
void for_each_member(tapos_response& obj, F f) {
    f("ref_block_num", obj.ref_block_num);
    f("ref_block_prefix", obj.ref_block_prefix);
    f("expiration", obj.expiration);
}
