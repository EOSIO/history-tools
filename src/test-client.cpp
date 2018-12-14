#include "test-common.hpp"

extern "C" void eosio_assert(uint32_t test, const char* msg) {
    if (!test)
        eosio_assert_message(test, msg, strlen(msg));
}

template <typename T>
void process(T&& reply) {
    std::vector<char> json;
    to_json(json, reply);
    set_output_data(json);
}

extern "C" void create_request() {
    auto  request = get_input_data();
    auto* pos     = request.data();
    auto* end     = pos + request.size();
    json_parser::skip_space(pos, end);
    json_parser::expect(pos, end, '[', "expected array");

    auto type = json_parser::parse<eosio::name>(pos, end);
    json_parser::expect(pos, end, ',', "expected ,");

    switch (type.value) {
    case "bal.mult.acc"_n.value: set_output_data(pack(json_parser::parse<balances_for_multiple_accounts_request>(pos, end))); break;
    case "bal.mult.tok"_n.value: set_output_data(pack(json_parser::parse<balances_for_multiple_tokens_request>(pos, end))); break;
    default: eosio_assert(false, "unsupported query");
    }

    json_parser::expect(pos, end, ']', "expected ]");
}

extern "C" void decode_reply() {
    auto reply      = get_input_data();
    auto reply_name = unpack<name>(reply);

    switch (reply_name.value) {
    case "bal.mult.acc"_n.value: return process(unpack<balances_for_multiple_accounts_response>(reply));
    case "bal.mult.tok"_n.value: return process(unpack<balances_for_multiple_tokens_response>(reply));
    }

    // todo: error on unrecognized
}
