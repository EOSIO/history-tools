// copyright defined in LICENSE.txt

#include "lib-database.hpp"
#include "lib-parse-json.hpp"
#include "lib-schema.hpp"
#include "lib-to-json.hpp"
#include "test-common.hpp"

// todo: move
extern "C" void eosio_assert(uint32_t test, const char* msg) {
    if (!test)
        eosio_assert_message(test, msg, strlen(msg));
}
extern "C" {
   __attribute__((eosio_wasm_entry))
   void initialize() {}

   void describe_request() { set_output_data(make_json_schema<token_request>()); }
   void describe_response() { set_output_data(make_json_schema<token_response>()); }

   void create_request() {
       set_output_data(pack(std::make_tuple("local"_n, "token"_n, parse_json<token_request>(get_input_data()))));
   }

   void decode_response() { //
       set_output_data(to_json(lvalue(unpack<token_response>(get_input_data()))));
   }
}
