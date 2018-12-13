#include "test-common.hpp"

extern "C" void eosio_assert(uint32_t test, const char* msg) {
    if (!test)
        eosio_assert_message(test, msg, strlen(msg));
}

void print_pad_name(name n) {
    char s[13] = {32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32};
    n.write_as_string(s, s + sizeof(s));
    print_range(s, s + sizeof(s));
}

void process(balances_for_multiple_accounts_response&& reply) {
    for (auto& row : reply.rows) {
        print("    ");
        print_pad_name(row.account);
        print(" ", asset_to_string(row.amount.quantity), "@", row.amount.contract, "\n");
    }
    if (reply.more)
        print("more: ", *reply.more, "\n");
}

extern "C" void create_request() {
    auto                                   x = get_input_data();
    balances_for_multiple_accounts_request request;
    char*                                  pos = x.data();
    char*                                  end = pos + x.size();
    json_parser::skip_space(pos, end);
    json_parser::parse_object(pos, end, [&](string_view key) {
        if (key == "max_block_index") {
            request.max_block_index = json_parser::parse_uint32(pos, end);
            return;
        }
        if (key == "code") {
            request.code = json_parser::parse_name(pos, end);
            return;
        }
        if (key == "sym") {
            request.sym = json_parser::parse_symbol_code(pos, end);
            return;
        }
        if (key == "first_account") {
            request.first_account = json_parser::parse_name(pos, end);
            return;
        }
        if (key == "last_account") {
            request.last_account = json_parser::parse_name(pos, end);
            return;
        }
        if (key == "max_results") {
            request.max_results = json_parser::parse_uint32(pos, end);
            return;
        }
        json_parser::skip_value(pos, end);
    });
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
