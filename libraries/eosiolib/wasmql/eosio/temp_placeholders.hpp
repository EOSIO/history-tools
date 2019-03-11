// copyright defined in LICENSE.txt

// todo: remove or replace everything in this file

#pragma once
#include <eosio/asset.hpp>
#include <eosio/datastream.hpp>
#include <eosio/fixed_bytes.hpp>
#include <eosio/tagged_variant.hpp>
#include <eosio/varint.hpp>

extern "C" void print_range(const char* begin, const char* end);

template <typename T>
T& lvalue(T&& v) {
    return v;
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
