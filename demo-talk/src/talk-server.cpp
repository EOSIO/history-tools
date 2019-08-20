#include "talk.hpp"

#include <eosio/database.hpp>
#include <eosio/input_output.hpp>

// Get first child with id >= min_id
std::optional<message> get_message(uint64_t parent, uint64_t min_id, const eosio::database_status& status) {
    auto s = query_database(eosio::query_contract_index64_range_code_table_scope_sk_pk{
        .snapshot_block = status.head,
        .first =
            {
                .code          = "talk"_n,
                .table         = "message"_n,
                .scope         = ""_n,
                .secondary_key = parent,
                .primary_key   = min_id,
            },
        .last =
            {
                .code          = "talk"_n,
                .table         = "message"_n,
                .scope         = ""_n,
                .secondary_key = parent,
                .primary_key   = 0xffff'ffff'ffff'ffffull,
            },
        .max_results = 10,
    });

    std::optional<message> result;

    // todo: handle case where all results are !present, but more results follow.
    eosio::for_each_query_result<eosio::contract_secondary_index_with_row<uint64_t>>(s, [&](auto& r) {
        if (r.present && r.row_present) {
            result.emplace();
            *r.row_value >> *result;
            return false;
        }
        return true;
    });

    return result;
}

// process this query
void process(get_messages_request& req, const eosio::database_status& status) {
    auto                  parents = std::move(req.begin.parent_ids);
    auto                  id      = req.begin.id;
    get_messages_response response;

    while (true) {
        if (response.messages.size() >= req.max_messages) {
            // Terminate search and mark resume point
            response.more = message_position{
                .parent_ids = std::move(parents),
                .id         = id,
            };
            break;
        }

        // Get next message at this level
        auto msg = get_message(parents.empty() ? 0 : parents.back(), id, status);

        if (msg) {
            // Found message
            response.messages.push_back(*msg);

            // Get its children
            parents.push_back(msg->id);
            id = 0;
        } else if (!parents.empty()) {
            // End of children; pop up a level
            id = parents.back() + 1;
            parents.pop_back();
        } else {
            // No more results
            break;
        }
    }

    // send the result
    eosio::set_output_data(pack(talk_query_response{std::move(response)}));
}

// initialize this WASM
extern "C" __attribute__((eosio_wasm_entry)) void initialize() {}

// wasm-ql calls this for each incoming query
extern "C" void run_query() {
    // deserialize the request
    auto request = eosio::unpack<talk_query_request>(eosio::get_input_data());

    // dispatch the request to the appropriate `process` overload
    std::visit([](auto& x) { process(x, eosio::get_database_status()); }, request.value);
}
