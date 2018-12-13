#include "test-common.hpp"

extern "C" void eosio_assert(uint32_t test, const char* msg) {
    if (!test)
        eosio_assert_message(test, msg, strlen(msg));
}

void process(balances_for_multiple_accounts_response&& reply) {
    std::vector<char> json;
    to_json(json, reply);
    set_output_data(json);
}

extern "C" void create_request() {
    auto request = json_parser::parse<balances_for_multiple_accounts_request>(get_input_data());
    set_output_data(pack(request));
}

extern "C" void decode_reply() {
    auto reply      = get_input_data();
    auto reply_name = unpack<name>(reply);

    switch (reply_name.value) {
    case "bal.mult.acc"_n.value: return process(unpack<balances_for_multiple_accounts_response>(reply));
    }

    // todo: error on unrecognized
}
