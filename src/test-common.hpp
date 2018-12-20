// copyright defined in LICENSE.txt

// todo: first vs. first_key, last vs. last_key
// todo: results vs. response vs. rows

#pragma once
#include <string_view>

extern "C" void print_range(const char* begin, const char* end);

namespace eosio {
void print(std::string_view sv) { print_range(sv.data(), sv.data() + sv.size()); }
} // namespace eosio

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
T& lvalue(T&& v) {
    return v;
}

// todo: remove this
template <typename T>
struct serial_wrapper {
    T value{};
};

// todo: remove this
template <typename DataStream>
DataStream& operator<<(DataStream& ds, serial_wrapper<checksum256>& obj) {
    ds.write(reinterpret_cast<char*>(obj.value.data()), obj.value.num_words() * sizeof(checksum256::word_t));
    return ds;
}

// todo: remove this
template <typename DataStream>
DataStream& operator>>(DataStream& ds, serial_wrapper<checksum256>& obj) {
    ds.read(reinterpret_cast<char*>(obj.value.data()), obj.value.num_words() * sizeof(checksum256::word_t));
    return ds;
}

bool increment(checksum256& v) {
    auto bytes = reinterpret_cast<char*>(v.data());
    for (int i = 0; i < 64; ++i) {
        auto& x = bytes[63 - i];
        if (++x)
            return false;
    }
    return true;
}

// todo: don't return static storage
// todo: replace with eosio functions when linker is improved
inline std::string_view asset_amount_to_string(const asset& v) {
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
    return std::string_view(result, pos - result);
}

// todo: don't return static storage
// todo: replace with eosio functions when linker is improved
inline const char* asset_to_string(const asset& v) {
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

template <typename Stream1, typename Stream2>
inline datastream<Stream1>& operator<<(datastream<Stream1>& ds, const datastream<Stream2>& obj) {
    unsigned_int size = obj.remaining();
    ds << size;
    ds.write(obj.pos(), size);
    return ds;
}

// todo: remove
template <typename Stream>
inline datastream<Stream>& operator>>(datastream<Stream>& ds, std::string_view& dest) {
    unsigned_int size;
    ds >> size;
    dest = std::string_view{ds.pos(), size};
    ds.skip(size);
    return ds;
}

// todo: remove
template <typename Stream>
inline datastream<Stream>& operator<<(datastream<Stream>& ds, const std::string_view& obj) {
    unsigned_int size = obj.size();
    ds << size;
    ds.write(obj.begin(), size);
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

inline void skip_space(char*& pos, char* end) {
    while (pos != end && (*pos == 0x09 || *pos == 0x0a || *pos == 0x0d || *pos == 0x20))
        ++pos;
}

// todo
inline void skip_value(char*& pos, char* end) {
    while (pos != end && *pos != ',' && *pos != '}')
        ++pos;
}

inline void expect(char*& pos, char* end, char ch, const char* msg) {
    eosio_assert(pos != end && *pos == ch, msg);
    ++pos;
    skip_space(pos, end);
}

inline void expect_end(char*& pos, char* end) { eosio_assert(pos == end, "expected end of json"); }

template <typename T>
inline void parse_json(T& obj, char*& pos, char* end);

// todo: escapes
inline void parse_json(std::string_view& result, char*& pos, char* end) {
    eosio_assert(pos != end && *pos++ == '"', "expected string");
    auto begin = pos;
    while (pos != end && *pos != '"')
        ++pos;
    auto e = pos;
    eosio_assert(pos != end && *pos++ == '"', "expected end of string");
    result = string_view(begin, e - begin);
}

inline void parse_json(uint32_t& result, char*& pos, char* end) {
    bool in_str = false;
    if (pos != end && *pos == '"') {
        in_str = true;
        skip_space(pos, end);
    }
    bool found = false;
    result     = 0;
    while (pos != end && *pos >= '0' && *pos <= '9') {
        result = result * 10 + *pos++ - '0';
        found  = true;
    }
    eosio_assert(found, "expected positive integer");
    skip_space(pos, end);
    if (in_str) {
        expect(pos, end, '"', "expected positive integer");
        skip_space(pos, end);
    }
}

inline void parse_json(name& result, char*& pos, char* end) {
    std::string_view sv;
    parse_json(sv, pos, end);
    result = name{sv};
}

inline void parse_json(symbol_code& result, char*& pos, char* end) {
    std::string_view sv;
    parse_json(sv, pos, end);
    result = symbol_code{sv};
}

inline void parse_json(serial_wrapper<checksum256>& result, char*& pos, char* end) {
    auto             bytes = reinterpret_cast<char*>(result.value.data());
    std::string_view sv;
    parse_json(sv, pos, end);
    eosio_assert(sv.size() == 64, "expected checksum256");
    auto p = sv.begin();
    for (int i = 0; i < 32; ++i) {
        auto get_digit = [&]() {
            auto ch = *p++;
            if (ch >= '0' && ch <= '9')
                return ch - '0';
            if (ch >= 'a' && ch <= 'f')
                return ch - 'a' + 10;
            if (ch >= 'A' && ch <= 'F')
                return ch - 'A' + 10;
            eosio_assert(false, "expected checksum256");
            return 0;
        };
        auto h   = get_digit();
        auto l   = get_digit();
        bytes[i] = (h << 4) | l;
    }
}

template <typename F>
inline void parse_object(char*& pos, char* end, F f) {
    expect(pos, end, '{', "expected {");
    while (true) {
        std::string_view key;
        parse_json(key, pos, end);
        expect(pos, end, ':', "expected :");
        f(key);
        if (pos != end && *pos == ',')
            ++pos;
        else
            break;
    }
    expect(pos, end, '}', "expected }");
}

template <typename T>
inline void parse_json(T& obj, char*& pos, char* end) {
    parse_object(pos, end, [&](std::string_view key) {
        bool found = false;
        for_each_member(obj, [&](std::string_view member_name, auto& member) {
            if (key == member_name) {
                parse_json(member, pos, end);
                found = true;
            }
        });
        if (!found)
            skip_value(pos, end);
    });
}

template <typename T>
inline T parse_json(std::vector<char>&& v) {
    char* pos = v.data();
    char* end = pos + v.size();
    skip_space(pos, end);
    T result;
    parse_json(result, pos, end);
    expect_end(pos, end);
    return result;
}

// todo: escape
// todo: handle non-utf8
inline void to_json(std::string_view sv, std::vector<char>& dest) {
    dest.push_back('"');
    dest.insert(dest.end(), sv.begin(), sv.end());
    dest.push_back('"');
}

inline void to_json(uint8_t value, std::vector<char>& dest) {
    char  s[4];
    char* ch = s;
    do {
        *ch++ = '0' + (value % 10);
        value /= 10;
    } while (value);
    std::reverse(s, ch);
    dest.insert(dest.end(), s, ch);
}

inline void to_json(uint32_t value, std::vector<char>& dest) {
    char  s[20];
    char* ch = s;
    do {
        *ch++ = '0' + (value % 10);
        value /= 10;
    } while (value);
    std::reverse(s, ch);
    dest.insert(dest.end(), s, ch);
}

inline void to_json(int64_t value, std::vector<char>& dest) {
    bool     neg = false;
    uint64_t u   = value;
    if (value < 0) {
        neg = true;
        u   = -value;
    }
    char  s[30];
    char* ch = s;
    do {
        *ch++ = '0' + (u % 10);
        u /= 10;
    } while (u);
    if (neg)
        *ch++ = '-';
    std::reverse(s, ch);
    dest.push_back('"');
    dest.insert(dest.end(), s, ch);
    dest.push_back('"');
}

inline void to_json(name value, std::vector<char>& dest) {
    char buffer[13];
    auto end = value.write_as_string(buffer, buffer + sizeof(buffer));
    dest.push_back('"');
    dest.insert(dest.end(), buffer, end);
    dest.push_back('"');
}

inline void append(std::vector<char>& dest, std::string_view sv) { dest.insert(dest.end(), sv.begin(), sv.end()); }

inline void to_json(symbol_code value, std::vector<char>& dest) {
    char buffer[10];
    dest.push_back('"');
    append(dest, std::string_view{buffer, size_t(value.write_as_string(buffer, buffer + sizeof(buffer)) - buffer)});
    dest.push_back('"');
}

inline void to_json(asset value, std::vector<char>& dest) {
    append(dest, "{\"symbol\":\"");
    char buffer[10];
    append(dest, std::string_view{buffer, size_t(value.symbol.code().write_as_string(buffer, buffer + sizeof(buffer)) - buffer)});
    append(dest, "\",\"precision\":");
    to_json(value.symbol.precision(), dest);
    append(dest, ",\"amount\":");
    to_json(value.amount, dest);
    append(dest, "}");
}

inline void to_json(extended_asset value, std::vector<char>& dest) {
    append(dest, "{\"contract\":");
    to_json(value.contract, dest);
    append(dest, ",\"symbol\":\"");
    char buffer[10];
    append(dest, std::string_view{buffer, size_t(value.quantity.symbol.code().write_as_string(buffer, buffer + sizeof(buffer)) - buffer)});
    append(dest, "\",\"precision\":");
    to_json(value.quantity.symbol.precision(), dest);
    append(dest, ",\"amount\":");
    to_json(value.quantity.amount, dest);
    append(dest, "}");
}

// todo: fix const
inline void to_json(serial_wrapper<checksum256>& value, std::vector<char>& dest) {
    static const char hex_digits[] = "0123456789ABCDEF";
    auto              bytes        = reinterpret_cast<const unsigned char*>(value.value.data());
    dest.push_back('"');
    auto pos = dest.size();
    dest.resize(pos + 64);
    for (int i = 0; i < 32; ++i) {
        dest[pos++] = hex_digits[bytes[i] >> 4];
        dest[pos++] = hex_digits[bytes[i] & 15];
    }
    dest.push_back('"');
}

template <typename T>
inline void to_json(std::optional<T>& obj, std::vector<char>& dest) {
    if (obj)
        to_json(*obj, dest);
    else
        append(dest, "null");
}

template <typename T>
inline void to_json(std::vector<T>& obj, std::vector<char>& dest) {
    dest.push_back('[');
    bool first = true;
    for (auto& v : obj) {
        if (!first)
            dest.push_back(',');
        first = false;
        to_json(v, dest);
    }
    dest.push_back(']');
}

template <typename T>
inline void to_json(T& obj, std::vector<char>& dest) {
    dest.push_back('{');
    bool first = true;
    for_each_member(obj, [&](std::string_view member_name, auto& member) {
        if (!first)
            dest.push_back(',');
        first = false;
        to_json(member_name, dest);
        dest.push_back(':');
        to_json(member, dest);
    });
    dest.push_back('}');
}

template <typename T>
inline std::vector<char> to_json(T& obj) {
    std::vector<char> result;
    to_json(obj, result);
    return result;
}

template <eosio::name::raw N, typename T>
struct named_type {
    static inline constexpr eosio::name name = eosio::name{N};
    using type                               = T;
};

// todo: compile-time check for duplicate keys
template <typename... NamedTypes>
struct named_variant {
    std::variant<typename NamedTypes::type...> value;
    static inline constexpr eosio::name        keys[] = {NamedTypes::name...};
};

template <size_t I, typename DataStream, typename... NamedTypes>
DataStream& deserialize_named_variant_impl(DataStream& ds, named_variant<NamedTypes...>& v, size_t i) {
    if constexpr (I < sizeof...(NamedTypes)) {
        if (i == I) {
            auto& q = v.value;
            auto& x = q.template emplace<I>();
            ds >> x;
            return ds;
        } else {
            return deserialize_named_variant_impl<I + 1>(ds, v, i);
        }
    } else {
        eosio_assert(false, "invalid variant index");
        return ds;
    }
}

template <typename DataStream, typename... NamedTypes>
DataStream& operator>>(DataStream& ds, named_variant<NamedTypes...>& v) {
    eosio::name name;
    ds >> name;
    for (size_t i = 0; i < sizeof...(NamedTypes); ++i)
        if (name == v.keys[i])
            return deserialize_named_variant_impl<0>(ds, v, i);
    eosio_assert(false, "invalid variant index name");
    return ds;
}

template <typename DataStream, typename... NamedTypes>
DataStream& operator<<(DataStream& ds, const named_variant<NamedTypes...>& v) {
    ds << named_variant<NamedTypes...>::keys[v.value.index()];
    std::visit([&](auto& v) { ds << v; }, v.value);
    return ds;
}

// todo: const v
template <typename... NamedTypes>
inline void to_json(named_variant<NamedTypes...>& v, std::vector<char>& dest) {
    dest.push_back('[');
    to_json(named_variant<NamedTypes...>::keys[v.value.index()], dest);
    dest.push_back(',');
    std::visit([&](auto& x) { to_json(x, dest); }, v.value);
    dest.push_back(']');
}

template <size_t I, typename... NamedTypes>
void parse_named_variant_impl(named_variant<NamedTypes...>& v, size_t i, char*& pos, char* end) {
    if constexpr (I < sizeof...(NamedTypes)) {
        if (i == I) {
            auto& q = v.value;
            auto& x = q.template emplace<I>();
            parse_json(x, pos, end);
        } else {
            return parse_named_variant_impl<I + 1>(v, i, pos, end);
        }
    } else {
        eosio_assert(false, "invalid variant index");
    }
}

template <typename... NamedTypes>
void parse_json(named_variant<NamedTypes...>& result, char*& pos, char* end) {
    skip_space(pos, end);
    expect(pos, end, '[', "expected array");

    eosio::name name;
    parse_json(name, pos, end);
    expect(pos, end, ',', "expected ,");

    for (size_t i = 0; i < sizeof...(NamedTypes); ++i) {
        if (name == named_variant<NamedTypes...>::keys[i]) {
            parse_named_variant_impl<0>(result, i, pos, end);
            expect(pos, end, ']', "expected ]");
            return;
        }
    }
    eosio_assert(false, "invalid variant index name");
}

struct outgoing_transfers_key {
    name                        account        = {};
    name                        contract       = {};
    uint32_t                    block_index    = {};
    serial_wrapper<checksum256> transaction_id = {};
    uint32_t                    action_index   = {};

    // todo: create a shortcut for defining this
    outgoing_transfers_key& operator++() {
        if (++action_index)
            return *this;
        if (!increment(transaction_id.value))
            return *this;
        if (++block_index)
            return *this;
        if (++contract.value)
            return *this;
        if (++account.value)
            return *this;
        return *this;
    }

    EOSLIB_SERIALIZE(outgoing_transfers_key, (account)(contract)(block_index)(transaction_id)(action_index))
};

template <typename F>
void for_each_member(outgoing_transfers_key& obj, F f) {
    f("account", obj.account);
    f("contract", obj.contract);
    f("block_index", obj.block_index);
    f("transaction_id", obj.transaction_id);
    f("action_index", obj.action_index);
}

// todo: row?
struct outgoing_transfers_row {
    outgoing_transfers_key key      = {};
    eosio::name            from     = {};
    eosio::name            to       = {};
    eosio::asset           quantity = {};
    std::string_view       memo     = {nullptr, 0};

    EOSLIB_SERIALIZE(outgoing_transfers_row, (key)(from)(to)(quantity)(memo))
};

template <typename F>
void for_each_member(outgoing_transfers_row& obj, F f) {
    f("key", obj.key);
    f("from", obj.from);
    f("to", obj.to);
    f("quantity", obj.quantity);
    f("memo", obj.memo);
}

// todo: version
// todo: max_block_index: head, irreversible options
// todo: share struct with incoming_transfers_request
struct outgoing_transfers_request {
    name                   request         = "out.xfer"_n; // todo: remove
    uint32_t               max_block_index = {};
    outgoing_transfers_key first_key       = {};
    outgoing_transfers_key last_key        = {};
    uint32_t               max_results     = {};
};

template <typename F>
void for_each_member(outgoing_transfers_request& obj, F f) {
    f("request", obj.request);
    f("max_block_index", obj.max_block_index);
    f("first_key", obj.first_key);
    f("last_key", obj.last_key);
    f("max_results", obj.max_results);
}

// todo: version
// todo: share struct with incoming_transfers_response
struct outgoing_transfers_response {
    std::vector<outgoing_transfers_row>   rows = {}; // todo name: rows?
    std::optional<outgoing_transfers_key> more = {};

    EOSLIB_SERIALIZE(outgoing_transfers_response, (rows)(more))
};

template <typename F>
void for_each_member(outgoing_transfers_response& obj, F f) {
    f("rows", obj.rows);
    f("more", obj.more);
}

// todo: version
// todo: max_block_index: head, irreversible options
struct balances_for_multiple_accounts_request {
    name        request         = "bal.mult.acc"_n; // todo: remove
    uint32_t    max_block_index = {};
    name        code            = {};
    symbol_code sym             = {};
    name        first_account   = {};
    name        last_account    = {};
    uint32_t    max_results     = {};
};

template <typename F>
void for_each_member(balances_for_multiple_accounts_request& obj, F f) {
    f("request", obj.request);
    f("max_block_index", obj.max_block_index);
    f("code", obj.code);
    f("sym", obj.sym);
    f("first_account", obj.first_account);
    f("last_account", obj.last_account);
    f("max_results", obj.max_results);
}

struct bfmt_key {
    symbol_code sym  = {};
    name        code = {};

    bfmt_key& operator++() {
        code = name{code.value + 1};
        if (!code.value)
            sym = symbol_code{sym.raw() + 1};
        return *this;
    }
};

template <typename F>
void for_each_member(bfmt_key& obj, F f) {
    f("sym", obj.sym);
    f("code", obj.code);
}

// todo: version
// todo: max_block_index: head, irreversible options
struct balances_for_multiple_tokens_request {
    name     request         = "bal.mult.tok"_n; // todo: remove
    uint32_t max_block_index = {};
    name     account         = {};
    bfmt_key first_key       = {};
    bfmt_key last_key        = {};
    uint32_t max_results     = {};
};

template <typename F>
void for_each_member(balances_for_multiple_tokens_request& obj, F f) {
    f("max_block_index", obj.max_block_index);
    f("account", obj.account);
    f("first_key", obj.first_key);
    f("last_key", obj.last_key);
    f("max_results", obj.max_results);
}

// todo: version
struct balances_for_multiple_accounts_response {
    struct row {
        name           account = {};
        extended_asset amount  = {};
    };

    std::vector<row>    rows = {};
    std::optional<name> more = {};

    EOSLIB_SERIALIZE(balances_for_multiple_accounts_response, (rows)(more))
};

template <typename F>
void for_each_member(balances_for_multiple_accounts_response::row& obj, F f) {
    f("account", obj.account);
    f("amount", obj.amount);
}

template <typename F>
void for_each_member(balances_for_multiple_accounts_response& obj, F f) {
    f("rows", obj.rows);
    f("more", obj.more);
}

// todo: version
struct balances_for_multiple_tokens_response {
    struct row {
        name           account = {};
        extended_asset amount  = {};
    };

    std::vector<row>        rows = {};
    std::optional<bfmt_key> more = {};

    EOSLIB_SERIALIZE(balances_for_multiple_tokens_response, (rows)(more))
};

template <typename F>
void for_each_member(balances_for_multiple_tokens_response::row& obj, F f) {
    f("account", obj.account);
    f("amount", obj.amount);
}

template <typename F>
void for_each_member(balances_for_multiple_tokens_response& obj, F f) {
    f("rows", obj.rows);
    f("more", obj.more);
}

using example_request = named_variant<
    named_type<"out.xfer"_n, outgoing_transfers_request>,                 //
    named_type<"bal.mult.acc"_n, balances_for_multiple_accounts_request>, //
    named_type<"bal.mult.tok"_n, balances_for_multiple_tokens_request>>;  //

using example_response = named_variant<
    named_type<"out.xfer"_n, outgoing_transfers_response>,                 //
    named_type<"bal.mult.acc"_n, balances_for_multiple_accounts_response>, //
    named_type<"bal.mult.tok"_n, balances_for_multiple_tokens_response>>;  //
