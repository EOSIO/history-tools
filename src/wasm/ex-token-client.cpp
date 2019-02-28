// copyright defined in LICENSE.txt

#include "test-common.hpp"
#include <eosio/database.hpp>
#include <eosio/parse-json.hpp>
#include <eosio/schema.hpp>
#include <eosio/to-json.hpp>

// todo: move
extern "C" void eosio_assert(uint32_t test, const char* msg) {
    if (!test)
        eosio_assert_message(test, msg, strlen(msg));
}

extern "C" void describe_request() { set_output_data(eosio::make_json_schema<token_request>()); }
extern "C" void describe_response() { set_output_data(eosio::make_json_schema<token_response>()); }

extern "C" void create_request() {
    set_output_data(pack(std::make_tuple("local"_n, "token"_n, eosio::parse_json<token_request>(get_input_data()))));
}

extern "C" void decode_response() { //
    set_output_data(to_json(lvalue(eosio::unpack<token_response>(get_input_data()))));
}
