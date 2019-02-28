// copyright defined in LICENSE.txt

// todo: remove or replace everything in this file

#pragma once
#include "lib-tagged-variant.hpp"
#include <eosiolib/asset.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/varint.hpp>

extern "C" void print_range(const char* begin, const char* end);

template <typename T>
T& lvalue(T&& v) {
    return v;
}

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

inline bool increment(eosio::checksum256& v) {
    if (++v.data()[1])
        return true;
    if (++v.data()[0])
        return true;
    return false;
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

