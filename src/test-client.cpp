#include "test-common.hpp"

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
    set_output_data(pack(balances_for_multiple_accounts_request{
        .max_block_index = 100'000'000,
        .code            = "eosio.token"_n,
        .sym             = symbol_code{"EOS"},
        .first_account   = "c"_n,
        .last_account    = "zzzzzzzzzzzzj"_n,
        .max_results     = 10,
    }));
}

extern "C" void decode_reply() {
    auto reply      = get_input_data();
    auto reply_name = unpack<name>(reply);

    switch (reply_name.value) {
    case "bal.mult.acc"_n.value: return process(unpack<balances_for_multiple_accounts_response>(reply));
    }

    // todo: error on unrecognized
}
