#include "talk.hpp"

#include <eosio/input_output.hpp>
#include <eosio/parse_json.hpp>
#include <eosio/schema.hpp>

// initialize this WASM
extern "C" __attribute__((eosio_wasm_entry)) void initialize() {}

// produce JSON schema for request
extern "C" void describe_query_request() { //
    eosio::set_output_data(eosio::make_json_schema<talk_query_request>());
}

// produce JSON schema for response
extern "C" void describe_query_response() { //
    eosio::set_output_data(eosio::make_json_schema<talk_query_response>());
}

// convert request from JSON to binary
extern "C" void create_query_request() {
    eosio::set_output_data(pack(std::make_tuple(
        "local"_n, // must be "local"
        "talk"_n,  // name of server WASM
        eosio::parse_json<talk_query_request>(eosio::get_input_data()))));
}

// convert response from binary to JSON
extern "C" void decode_query_response() { //
    eosio::set_output_data(to_json(eosio::unpack<talk_query_response>(eosio::get_input_data())));
}
