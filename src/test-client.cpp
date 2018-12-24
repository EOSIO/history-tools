// copyright defined in LICENSE.txt

#include "lib-database.hpp"
#include "lib-parse-json.hpp"
#include "lib-to-json.hpp"
#include "test-common.hpp"

extern "C" void eosio_assert(uint32_t test, const char* msg) {
    if (!test)
        eosio_assert_message(test, msg, strlen(msg));
}

// static char      mem_pool[64 * 1024 * 1024];
// static char*     next_addr = mem_pool;
// extern "C" void* malloc(size_t s) {
//     char* result = (char*)(((int)next_addr + 0xf) & 0xffff'fff0);
//     next_addr    = result + s;
//     return result;
// }
// extern "C" void* calloc(size_t a, size_t b) { return malloc(a * b); }
// extern "C" void* realloc(void*, size_t) {
//     eosio_assert(false, "realloc not implemented");
//     return nullptr;
// }
// extern "C" void free(void*) {}

extern "C" void create_request() { //
    // 19k
    set_output_data(pack(parse_json<example_request>(get_input_data())));

    // 16k
    // parse_json<example_request>(get_input_data());

    // 14k
    // parse_json<token_transfer_request>(get_input_data());
    // parse_json<balances_for_multiple_accounts_request>(get_input_data());
    // parse_json<balances_for_multiple_tokens_request>(get_input_data());

    // 7.9k
    // parse_json<token_transfer_request>(get_input_data());

    // 5k
    // parse_json<balances_for_multiple_accounts_request>(get_input_data());

    // 6.6k
    // parse_json<balances_for_multiple_tokens_request>(get_input_data());
}

extern "C" void decode_response() {
    // 18k
    set_output_data(to_json(lvalue(unpack<example_response>(get_input_data()))));

    // 17k
    // set_output_data(to_json(lvalue(unpack<token_transfer_response>(get_input_data()))));
    // set_output_data(to_json(lvalue(unpack<balances_for_multiple_accounts_response>(get_input_data()))));
    // set_output_data(to_json(lvalue(unpack<balances_for_multiple_tokens_response>(get_input_data()))));

    // 12k
    // set_output_data(to_json(lvalue(unpack<token_transfer_response>(get_input_data()))));

    // 8.1k
    // set_output_data(to_json(lvalue(unpack<balances_for_multiple_accounts_response>(get_input_data()))));

    // 8.9k
    // set_output_data(to_json(lvalue(unpack<balances_for_multiple_tokens_response>(get_input_data()))));

    // 5.4k
    // unpack<example_response>(get_input_data());

    // 4.9k
    // unpack<token_transfer_response>(get_input_data());
    // unpack<balances_for_multiple_accounts_response>(get_input_data());
    // unpack<balances_for_multiple_tokens_response>(get_input_data());

    // 3.1k
    // return unpack<token_transfer_response>(get_input_data());

    // 2.3k
    // return unpack<balances_for_multiple_accounts_response>(get_input_data());

    // 2.5k
    // return unpack<balances_for_multiple_tokens_response>(get_input_data());
}
