// copyright defined in LICENSE.txt

#include "chain.hpp"
#include <eosio/input-output.hpp>
#include <eosio/parse-json.hpp>
#include <eosio/schema.hpp>

// todo: remove "uint64_t, uint64_t, uint64_t" after CDT changes

extern "C" void describe_query_request(uint64_t, uint64_t, uint64_t) {
    eosio::set_output_data(eosio::make_json_schema<chain_query_request>());
}
extern "C" void describe_query_response(uint64_t, uint64_t, uint64_t) {
    eosio::set_output_data(eosio::make_json_schema<chain_query_response>());
}

extern "C" void create_query_request(uint64_t, uint64_t, uint64_t) {
    eosio::set_output_data(pack(std::make_tuple("local"_n, "chain"_n, eosio::parse_json<chain_query_request>(eosio::get_input_data()))));
}

extern "C" void decode_query_response(uint64_t, uint64_t, uint64_t) { //
    eosio::set_output_data(to_json(lvalue(eosio::unpack<chain_query_response>(eosio::get_input_data()))));
}
