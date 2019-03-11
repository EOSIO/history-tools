// copyright defined in LICENSE.txt

#include "chain.hpp"
#include <eosio/database.hpp>
#include <eosio/input_output.hpp>

void process(block_info_request& req, const eosio::database_status& status) {
    auto s = query_database(eosio::query_block_info_range_index{
        .first       = get_block_num(req.first, status),
        .last        = get_block_num(req.last, status),
        .max_results = req.max_results,
    });

    block_info_response response;
    eosio::for_each_query_result<eosio::block_info>(s, [&](eosio::block_info& b) {
        response.more = eosio::make_absolute_block(b.block_num + 1);
        response.blocks.push_back(b);
        return true;
    });
    eosio::set_output_data(pack(chain_query_response{std::move(response)}));
}

void process(tapos_request& req, const eosio::database_status& status) {
    auto s = query_database(eosio::query_block_info_range_index{
        .first       = get_block_num(req.ref_block, status),
        .last        = get_block_num(req.ref_block, status),
        .max_results = 1,
    });

    tapos_response response;
    eosio::for_each_query_result<eosio::block_info>(s, [&](eosio::block_info& b) {
        uint32_t x                = b.block_id.data()[0] >> 32;
        response.ref_block_num    = b.block_num;
        response.ref_block_prefix = (x << 24) | ((x & 0xff00) << 8) | ((x & 0xff0000) >> 8) | (x >> 24);
        response.expiration       = b.timestamp;
        response.expiration.slot += req.expire_seconds * 2;
        return true;
    });

    eosio::set_output_data(pack(chain_query_response{std::move(response)}));
}

void process(account_request& req, const eosio::database_status& status) {
    auto s = query_database(eosio::query_account_range_name{
        .max_block   = get_block_num(req.max_block, status),
        .first       = req.first,
        .last        = req.last,
        .max_results = req.max_results,
    });

    account_response response;
    eosio::for_each_query_result<eosio::account>(s, [&](eosio::account& a) {
        response.more = eosio::name{a.name.value + 1};
        if (a.present) {
            if (!req.include_abi)
                a.abi = {nullptr, 0};
            if (!req.include_code)
                a.code = {nullptr, 0};
            response.accounts.push_back(a);
        }
        return true;
    });
    eosio::set_output_data(pack(chain_query_response{std::move(response)}));
}

void process(abis_request& req, const eosio::database_status& status) {
    abis_response response;
    for (auto name : req.names) {
        auto s     = query_database(eosio::query_account_range_name{
            .max_block   = get_block_num(req.max_block, status),
            .first       = name,
            .last        = name,
            .max_results = 1,
        });
        bool found = false;
        eosio::for_each_query_result<eosio::account>(s, [&](eosio::account& a) {
            if (a.present) {
                found = true;
                response.abis.push_back(name_abi{a.name, true, a.abi});
            }
            return true;
        });
        if (!found)
            response.abis.push_back(name_abi{name, false, {nullptr, 0}});
    }
    eosio::set_output_data(pack(chain_query_response{std::move(response)}));
}

extern "C" __attribute__((eosio_wasm_entry)) void initialize() {}

extern "C" void run_query() {
    auto request = eosio::unpack<chain_query_request>(eosio::get_input_data());
    std::visit([](auto& x) { process(x, eosio::get_database_status()); }, request.value);
}
