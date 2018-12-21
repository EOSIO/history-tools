// copyright defined in LICENSE.txt

// todo: remove or replace everything in this file

#pragma once
#include <eosiolib/asset.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/varint.hpp>

template <typename T>
T& lvalue(T&& v) {
    return v;
}

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
    auto end = eosio::name{n}.write_as_string(buffer, buffer + sizeof(buffer));
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

namespace eosio {
void print(std::string_view sv) { print_range(sv.data(), sv.data() + sv.size()); }
} // namespace eosio

template <typename T>
struct serial_wrapper {
    T value{};
};

template <typename DataStream>
DataStream& operator<<(DataStream& ds, serial_wrapper<eosio::checksum256>& obj) {
    ds.write(reinterpret_cast<char*>(obj.value.data()), obj.value.num_words() * sizeof(eosio::checksum256::word_t));
    return ds;
}

template <typename DataStream>
DataStream& operator>>(DataStream& ds, serial_wrapper<eosio::checksum256>& obj) {
    ds.read(reinterpret_cast<char*>(obj.value.data()), obj.value.num_words() * sizeof(eosio::checksum256::word_t));
    return ds;
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

template <typename Stream>
inline datastream<Stream>& operator>>(datastream<Stream>& ds, std::string_view& dest) {
    unsigned_int size;
    ds >> size;
    dest = std::string_view{ds.pos(), size};
    ds.skip(size);
    return ds;
}

template <typename Stream>
inline datastream<Stream>& operator<<(datastream<Stream>& ds, const std::string_view& obj) {
    unsigned_int size = obj.size();
    ds << size;
    ds.write(obj.begin(), size);
    return ds;
}

} // namespace eosio

bool increment(eosio::checksum256& v) {
    auto bytes = reinterpret_cast<char*>(v.data());
    for (int i = 0; i < 64; ++i) {
        auto& x = bytes[63 - i];
        if (++x)
            return false;
    }
    return true;
}

// todo: don't return static storage
inline std::string_view asset_amount_to_string(const eosio::asset& v) {
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
inline const char* asset_to_string(const eosio::asset& v) {
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
