// copyright defined in LICENSE.txt

#include "ex-chain.hpp"
#include "lib-database.hpp"
#include "test-common.hpp"

void process(block_info_request& req) {
    print("    block_info_request\n");
    auto s = exec_query(query_block_info_range_index{
        .first       = req.first,
        .last        = req.last,
        .max_results = req.max_results,
    });

    block_info_response response;
    for_each_query_result<block_info>(s, [&](block_info& b) {
        response.more = b.block_num + 1;
        response.blocks.push_back(b);
        return true;
    });
    set_output_data(pack(example_response{std::move(response)}));
    print("\n");
}

void process(tapos_request& req) {
    print("    tapos_request\n");
    auto s = exec_query(query_block_info_range_index{
        .first       = req.ref_block,
        .last        = req.ref_block,
        .max_results = 1,
    });

    tapos_response response;
    for_each_query_result<block_info>(s, [&](block_info& b) {
        uint32_t x                = b.block_id.value.data()[0] >> 32;
        response.ref_block_num    = b.block_num;
        response.ref_block_prefix = (x << 24) | ((x & 0xff00) << 8) | ((x & 0xff0000) >> 8) | (x >> 24);
        response.expiration       = b.timestamp;
        response.expiration.slot += req.expire_seconds * 2;
        return true;
    });

    set_output_data(pack(example_response{std::move(response)}));
    print("\n");
}

void process(account_request& req) {
    print("    account\n");
    auto s = exec_query(query_account_range_name{
        .max_block   = req.max_block,
        .first       = req.first,
        .last        = req.last,
        .max_results = req.max_results,
    });

    account_response response;
    for_each_query_result<account>(s, [&](account& a) {
        response.more = name{a.name.value + 1};
        if (a.present) {
            if (!req.include_abi)
                a.abi = {nullptr, 0};
            if (!req.include_code)
                a.code = {nullptr, 0};
            response.accounts.push_back(a);
        }
        return true;
    });
    set_output_data(pack(example_response{std::move(response)}));
    print("\n");
}

void process(abis_request& req) {
    print("    abis\n");
    abis_response response;
    for (auto name : req.names) {
        auto s     = exec_query(query_account_range_name{
            .max_block   = req.max_block,
            .first       = name,
            .last        = name,
            .max_results = 1,
        });
        bool found = false;
        for_each_query_result<account>(s, [&](account& a) {
            if (a.present) {
                found = true;
                response.abis.push_back(name_abi{a.name, true, a.abi});
            }
            return true;
        });
        if (!found)
            response.abis.push_back(name_abi{name, false, {nullptr, 0}});
    }
    set_output_data(pack(example_response{std::move(response)}));
    print("\n");
}
