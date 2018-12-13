#include "test-common.hpp"

extern "C" void set_result(const char* begin, const char* end);

inline void set_result(const std::vector<char>& v) {
    // for(auto b: v)
    //     print("    ", int(uint8_t(b)), "\n");
    set_result(v.data(), v.data() + v.size());
}
inline void set_result(const std::string_view& v) { set_result(v.data(), v.data() + v.size()); }

extern "C" void create_request() {
    print("create_request()\n");
    set_result(pack(balances_for_multiple_accounts_request{
        .max_block_index = 100'000'000,
        .code            = "eosio.token"_n,
        .sym             = symbol_code{"EOS"},
        .first_account   = "c"_n,
        .last_account    = "zzzzzzzzzzzzj"_n,
        .max_results     = 10,
    }));
}
