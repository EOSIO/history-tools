#include "test-common.hpp"

extern "C" void eosio_assert(uint32_t test, const char* msg) {
    if (!test)
        eosio_assert_message(test, msg, strlen(msg));
}

extern "C" void create_request() { //
    set_output_data(pack(parse_json<example_request>(get_input_data())));
}

extern "C" void decode_response() {
    std::vector<char> json;
    to_json(json, lvalue(unpack<example_response>(get_input_data())));
    set_output_data(json);
}
