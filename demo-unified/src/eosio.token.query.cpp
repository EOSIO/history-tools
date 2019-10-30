#include "eosio.token.hpp"

std::vector<eosio::asset> token::gettoks(eosio::name owner) {
    accounts                  accounts(get_self(), owner.value);
    std::vector<eosio::asset> result;
    for (auto& acc : accounts) {
        print(owner, " ", acc.balance, "\n");
        result.push_back(acc.balance);
    }
    return result;
}

extern "C" {
[[eosio::wasm_entry]] void initialize() { eosio::print("aaaa\n"); }

[[eosio::wasm_entry]] void run_query() {
    eosio::print("bbbb\n");
    eosio::set_output_data(std::string("cccc\n"));
    eosio::print("###", eosio::get_input_data_str(), "###\n");

    token contr("eosio.token"_n, "eosio.token"_n, eosio::datastream<const char*>(nullptr, 0));
    contr.gettoks("eosio"_n);
    eosio::print("dddd\n");
}
}