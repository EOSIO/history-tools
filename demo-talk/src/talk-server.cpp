#include "talk.hpp"

#include <eosio/database.hpp>
#include <eosio/input_output.hpp>

// process this query
void process(get_messages_request& req, const eosio::database_status& status) {
    get_messages_response response;

    // send the result to the requestor
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
