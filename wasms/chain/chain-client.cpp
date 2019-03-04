// copyright defined in LICENSE.txt

#include "chain.hpp"
#include <eosio/database.hpp>
#include <eosio/input-output.hpp>
#include <eosio/parse-json.hpp>
#include <eosio/schema.hpp>
#include <eosio/to-json.hpp>

extern "C" void describe_request() { eosio::set_output_data(eosio::make_json_schema<chain_request>()); }
extern "C" void describe_response() { eosio::set_output_data(eosio::make_json_schema<chain_response>()); }

extern "C" void create_request() {
    eosio::set_output_data(pack(std::make_tuple("local"_n, "chain"_n, eosio::parse_json<chain_request>(eosio::get_input_data()))));
}

extern "C" void decode_response() { //
    eosio::set_output_data(to_json(lvalue(eosio::unpack<chain_response>(eosio::get_input_data()))));
}
