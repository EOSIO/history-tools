// copyright defined in LICENSE.txt

#include <eosiolib/action.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/varint.hpp>
#include <memory>
#include <string>
#include <vector>

#define ripemd160 internal_ripemd160
#include "../external/abieos/src/ripemd160.hpp"
#undef ripemd160

using namespace eosio;
using namespace std;

extern "C" void* memcpy(void* __restrict dest, const void* __restrict src, size_t size) {
    auto d = reinterpret_cast<char*>(dest);
    auto s = reinterpret_cast<const char*>(src);
    while (size--)
        *d++ = *s++;
    return dest;
}

extern "C" void* memmove(void* dest, const void* src, size_t size) {
    auto d = reinterpret_cast<char*>(dest);
    auto s = reinterpret_cast<const char*>(src);
    if (d < s) {
        while (size--)
            *d++ = *s++;
    } else {
        for (size_t p = 0; p < size; ++p)
            d[size - p - 1] = s[size - p - 1];
    }
    return dest;
}

extern "C" void* memset(void* dest, int v, size_t size) {
    auto d = reinterpret_cast<char*>(dest);
    while (size--)
        *d++ = v;
    return dest;
}

extern "C" void print_range(const char* begin, const char* end);
extern "C" void prints(const char* cstr) { print_range(cstr, cstr + strlen(cstr)); }
extern "C" void prints_l(const char* cstr, uint32_t len) { print_range(cstr, cstr + len); }

extern "C" void printn(uint64_t n) {
    char buffer[13];
    auto end = name{n}.write_as_string(buffer, buffer + sizeof(buffer));
    print_range(buffer, end);
}

extern "C" void printui(uint64_t value) {
    char  s[21];
    char* ch = s;
    do {
        *ch++ = '0' + (value % 10);
        value /= 10;
    } while (value);
    std::reverse(s, ch);
    *ch = 0;
    print_range(s, ch);
}

extern "C" void printi(int64_t value) {
    if (value < 0) {
        prints("-");
        printui(-value);
    } else
        printui(value);
}

// todo: remove this
template <typename T>
struct serial_wrapper {
    T value{};
};

// todo: remove this
template <typename DataStream>
DataStream& operator<<(DataStream& ds, const serial_wrapper<checksum256>& obj) {
    eosio_assert(false, "oops");
    return ds;
}

// todo: remove this
template <typename DataStream>
DataStream& operator>>(DataStream& ds, serial_wrapper<checksum256>& obj) {
    ds.read(reinterpret_cast<char*>(obj.value.data()), obj.value.num_words() * sizeof(checksum256::word_t));
    return ds;
}

// todo: don't return static storage
// todo: replace with eosio functions when linker is improved
const char* asset_to_string(const asset& v) {
    static char result[1000];
    auto        pos = result;
    uint64_t    amount;
    if (v.amount < 0)
        amount = -v.amount;
    else
        amount = v.amount;
    uint8_t precision = v.symbol.precision();
    if (precision) {
        while (precision--) {
            *pos++ = '0' + amount % 10;
            amount /= 10;
        }
        *pos++ = '.';
    }
    do {
        *pos++ = '0' + amount % 10;
        amount /= 10;
    } while (amount);
    if (v.amount < 0)
        *pos++ = '-';
    std::reverse(result, pos);
    *pos++ = ' ';

    auto sc = v.symbol.code().raw();
    while (sc > 0) {
        *pos++ = char(sc & 0xFF);
        sc >>= 8;
    }

    *pos++ = 0;
    return result;
}

namespace eosio {
template <typename Stream>
inline datastream<Stream>& operator>>(datastream<Stream>& ds, datastream<Stream>& dest) {
    unsigned_int size;
    ds >> size;
    dest = datastream<Stream>{ds.pos(), size};
    ds.skip(size);
    return ds;
}
} // namespace eosio

typedef void* cb_alloc_fn(void* cb_alloc_data, size_t size);

enum class transaction_status : uint8_t {
    executed  = 0, // succeed, no error handler executed
    soft_fail = 1, // objectively failed (not executed), error handler executed
    hard_fail = 2, // objectively failed and error handler objectively failed thus no state change
    delayed   = 3, // transaction delayed/deferred/scheduled for future execution
    expired   = 4, // transaction expired and storage space refunded to user
};

struct contract_row {
    uint32_t                block_index = 0;
    bool                    present     = false;
    name                    code;
    uint64_t                scope;
    name                    table;
    uint64_t                primary_key = 0;
    name                    payer;
    datastream<const char*> value{nullptr, 0};
};

struct action_trace {
    uint32_t                    block_index             = {};
    serial_wrapper<checksum256> transaction_id          = {};
    uint32_t                    action_index            = {};
    uint32_t                    parent_action_index     = {};
    ::transaction_status        transaction_status      = {};
    eosio::name                 receipt_receiver        = {};
    serial_wrapper<checksum256> receipt_act_digest      = {};
    uint64_t                    receipt_global_sequence = {};
    uint64_t                    receipt_recv_sequence   = {};
    unsigned_int                receipt_code_sequence   = {};
    unsigned_int                receipt_abi_sequence    = {};
    eosio::name                 account                 = {};
    eosio::name                 name                    = {};
    datastream<const char*>     data                    = {nullptr, 0};
    bool                        context_free            = {};
    int64_t                     elapsed                 = {};

    EOSLIB_SERIALIZE(
        action_trace, (block_index)(transaction_id)(action_index)(parent_action_index)(transaction_status)(receipt_receiver)(
                          receipt_act_digest)(receipt_global_sequence)(receipt_recv_sequence)(receipt_code_sequence)(receipt_abi_sequence)(
                          account)(name)(data)(context_free)(elapsed))
};

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

html operator+(std::string_view lhs, const html& rhs) {
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

html to_html(const public_key& key) {
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

// todo: version
// todo: max_block_index: head, irreversible options
struct balances_for_multiple_accounts_request {
    name        request         = "bal.mult.acc"_n;
    uint32_t    max_block_index = {};
    name        code            = {};
    symbol_code sym             = {};
    name        first_account   = {};
    name        last_account    = {};
    uint32_t    max_results     = {};
};

// todo: version
struct balances_for_multiple_accounts_response {
    struct row {
        name           account = {};
        extended_asset amount  = {};
    };

    name                request = "bal.mult.acc"_n;
    std::vector<row>    rows    = {};
    std::optional<name> more    = {};

    EOSLIB_SERIALIZE(balances_for_multiple_accounts_response, (request)(rows)(more))
};
