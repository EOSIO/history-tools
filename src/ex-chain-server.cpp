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
        response.more = b.block_index + 1;
        response.blocks.push_back(b);
        return true;
    });
    set_output_data(pack(example_response{std::move(response)}));
    print("\n");
}
