// copyright defined in LICENSE.txt

// todo: first vs. first_key, last vs. last_key
// todo: results vs. response vs. rows

#pragma once
#include "ex-chain.hpp"
#include "ex-token.hpp"
#include "lib-tagged-variant.hpp"

#include <string_view>

#include <eosiolib/action.hpp>
#include <eosiolib/asset.hpp>
#include <memory>
#include <string>
#include <vector>

#define ripemd160 internal_ripemd160
#include "../external/abieos/src/ripemd160.hpp"
#undef ripemd160

using namespace eosio;
using namespace std;

typedef void* cb_alloc_fn(void* cb_alloc_data, size_t size);

struct html {
    std::vector<char> v;

    html() = default;

    html(std::string_view sv)
        : v(sv.begin(), sv.end()) {}

    template <int n>
    html(const char (&s)[n])
        : v(s, s + n) {}

    html& operator+=(const html& src) {
        v.insert(v.end(), src.v.begin(), src.v.end());
        return *this;
    }

    html replace(std::string_view pattern, std::string_view replacement) const {
        html  result;
        auto* p = v.data();
        auto* e = p + v.size();
        while (p != e) {
            if (e - p >= pattern.size() && !strncmp(p, pattern.begin(), pattern.size())) {
                result.v.insert(result.v.end(), replacement.begin(), replacement.end());
                p += pattern.size();
            } else
                result.v.push_back(*p++);
        }
        return result;
    }

    html replace(std::string_view pattern, const html& replacement) const {
        return replace(pattern, std::string_view{replacement.v.data(), replacement.v.size()});
    }

    html replace(std::string_view pattern, name n) const {
        char s[13];
        auto e = n.write_as_string(s, s + sizeof(s));
        return replace(pattern, std::string_view{s, size_t(e - s)});
    }

    html replace(std::string_view pattern, uint32_t value) const {
        char  s[21];
        char* ch = s;
        do {
            *ch++ = '0' + (value % 10);
            value /= 10;
        } while (value);
        std::reverse(s, ch);
        return replace(pattern, std::string_view{s, size_t(ch - s)});
    }

    html replace(std::string_view pattern, uint16_t value) const { return replace(pattern, uint32_t(value)); }

    template <typename T>
    html replace(std::string_view pattern, const T& obj) const {
        return replace(pattern, to_html(obj));
    }
};

inline html operator+(std::string_view lhs, const html& rhs) {
    html result(lhs);
    result += rhs;
    return result;
}

template <typename T>
html to_html(const std::vector<T>& v) {
    html result;
    for (auto& x : v)
        result += to_html(x);
    return result;
}

inline constexpr char base58_chars[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

inline constexpr auto create_base58_map() {
    std::array<int8_t, 256> base58_map{{0}};
    for (unsigned i = 0; i < base58_map.size(); ++i)
        base58_map[i] = -1;
    for (unsigned i = 0; i < sizeof(base58_chars); ++i)
        base58_map[base58_chars[i]] = i;
    return base58_map;
}
inline constexpr auto base58_map = create_base58_map();

template <auto size>
html binary_to_base58(const std::array<char, size>& bin) {
    html result;
    for (auto byte : bin) {
        int carry = byte;
        for (auto& result_digit : result.v) {
            int x        = (base58_map[result_digit] << 8) + carry;
            result_digit = base58_chars[x % 58];
            carry        = x / 58;
        }
        while (carry) {
            result.v.push_back(base58_chars[carry % 58]);
            carry = carry / 58;
        }
    }
    for (auto byte : bin)
        if (byte)
            break;
        else
            result.v.push_back('1');
    std::reverse(result.v.begin(), result.v.end());
    return result;
}

template <size_t size, int suffix_size>
inline auto digest_suffix_ripemd160(const std::array<char, size>& data, const char (&suffix)[suffix_size]) {
    std::array<unsigned char, 20>       digest;
    internal_ripemd160::ripemd160_state self;
    internal_ripemd160::ripemd160_init(&self);
    internal_ripemd160::ripemd160_update(&self, (uint8_t*)data.data(), data.size());
    internal_ripemd160::ripemd160_update(&self, (uint8_t*)suffix, suffix_size - 1);
    if (!internal_ripemd160::ripemd160_digest(&self, digest.data()))
        ; // !!! throw error("ripemd failed");
    return digest;
}

template <typename Key, int suffix_size>
html key_to_string(const Key& key, const char (&suffix)[suffix_size], std::string_view prefix) {
    // todo: fix digest
    // static constexpr auto         size        = std::tuple_size_v<decltype(Key::data)>;
    // auto                          ripe_digest = digest_suffix_ripemd160(key.data, suffix);
    // std::array<uint8_t, size + 4> whole;
    // memcpy(whole.data(), key.data.data(), size);
    // memcpy(whole.data() + size, ripe_digest.data(), 4);
    return prefix + binary_to_base58(key.data);
}

namespace eosio {

enum class key_type : uint8_t {
    k1 = 0,
    r1 = 1,
};

inline html to_html(const public_key& key) {
    if (key.type.value == (uint8_t)key_type::k1) {
        return key_to_string(key, "K1", "PUB_K1_");
    } else if (key.type.value == (uint8_t)key_type::r1) {
        return key_to_string(key, "R1", "PUB_R1_");
    } else {
        return "unrecognized public key format";
    }
}

} // namespace eosio

extern "C" void get_input_data(void* cb_alloc_data, cb_alloc_fn* cb_alloc);

template <typename Alloc_fn>
inline void get_input_data(Alloc_fn alloc_fn) {
    get_input_data(&alloc_fn, [](void* cb_alloc_data, size_t size) -> void* { //
        return (*reinterpret_cast<Alloc_fn*>(cb_alloc_data))(size);
    });
}

inline std::vector<char> get_input_data() {
    std::vector<char> result;
    get_input_data([&result](size_t size) {
        result.resize(size);
        return result.data();
    });
    return result;
}

extern "C" void set_output_data(const char* begin, const char* end);
inline void     set_output_data(const std::vector<char>& v) { set_output_data(v.data(), v.data() + v.size()); }
inline void     set_output_data(const std::string_view& v) { set_output_data(v.data(), v.data() + v.size()); }

using chain_request = tagged_variant<                //
    serialize_tag_as_name,                           //
    tagged_type<"block.info"_n, block_info_request>, //
    tagged_type<"tapos"_n, tapos_request>,           //
    tagged_type<"account"_n, account_request>,       //
    tagged_type<"abis"_n, abis_request>>;            //

using chain_response = tagged_variant<                //
    serialize_tag_as_name,                            //
    tagged_type<"block.info"_n, block_info_response>, //
    tagged_type<"tapos"_n, tapos_response>,           //
    tagged_type<"account"_n, account_response>,       //
    tagged_type<"abis"_n, abis_response>>;            //

using token_request = tagged_variant<                                      //
    serialize_tag_as_name,                                                 //
    tagged_type<"transfer"_n, token_transfer_request>,                     //
    tagged_type<"bal.mult.acc"_n, balances_for_multiple_accounts_request>, //
    tagged_type<"bal.mult.tok"_n, balances_for_multiple_tokens_request>>;  //

using token_response = tagged_variant<                                      //
    serialize_tag_as_name,                                                  //
    tagged_type<"transfer"_n, token_transfer_response>,                     //
    tagged_type<"bal.mult.acc"_n, balances_for_multiple_accounts_response>, //
    tagged_type<"bal.mult.tok"_n, balances_for_multiple_tokens_response>>;  //
