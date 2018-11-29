// copyright defined in LICENSE.txt

#include <eosiolib/asset.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/varint.hpp>
#include <memory>
#include <string>
#include <vector>

using namespace std;

extern "C" void printss(const char* begin, const char* end);
extern "C" void printi32(int32_t);

extern "C" void* memcpy(void* __restrict dest, const void* __restrict src, size_t size) {
    auto d = reinterpret_cast<char*>(dest);
    auto s = reinterpret_cast<const char*>(src);
    while (size--)
        *d++ = *s++;
    return dest;
}

extern "C" void* memset(void* dest, int v, size_t size) {
    auto d = reinterpret_cast<char*>(dest);
    while (size--)
        *d++ = v;
    return dest;
}

inline void print() {}

template <int size, typename... Args>
inline void print(const char (&s)[size], Args&&... args);

template <typename... Args>
inline void print(const string& s, Args&&... args);

template <typename... Args>
inline void print(const std::vector<char>& s, Args&&... args);

template <typename... Args>
inline void print(int32_t i, Args&&... args);

template <int size, typename... Args>
inline void print(const char (&s)[size], Args&&... args) {
    printss(s, s + size);
    print(std::forward<Args>(args)...);
}

template <typename... Args>
inline void print(const string& s, Args&&... args) {
    printss(s.c_str(), s.c_str() + s.size());
    print(std::forward<Args>(args)...);
}

template <typename... Args>
inline void print(const std::vector<char>& s, Args&&... args) {
    printss(s.data(), s.data() + s.size());
    print(std::forward<Args>(args)...);
}

template <typename... Args>
inline void print(int32_t i, Args&&... args) {
    printi32(i);
    print(std::forward<Args>(args)...);
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

struct db_result {
    uint32_t                       block_index = 0;
    bool                           present     = false;
    eosio::name                    code;
    eosio::name                    table;
    eosio::name                    scope;
    uint64_t                       primary_key = 0;
    eosio::name                    payer;
    eosio::datastream<const char*> value{nullptr, 0};
};

extern "C" void testdb(void* cb_alloc_data, cb_alloc_fn* cb_alloc);

template <typename Alloc_fn>
inline void testdb(Alloc_fn alloc_fn) {
    testdb(&alloc_fn, [](void* cb_alloc_data, size_t size) -> void* { //
        return (*reinterpret_cast<Alloc_fn*>(cb_alloc_data))(size);
    });
}

inline std::vector<char> testdb() {
    std::vector<char> result;
    testdb([&result](size_t size) {
        print("in callback: ", size, "\n");
        result.resize(size);
        return result.data();
    });
    return result;
}

extern "C" void startup() {
    print("\nstart wasm\n");
    auto s = testdb();
    print("s.size(): ", s.size(), "\n");
    eosio::datastream<const char*> ds(s.data(), s.size());
    std::vector<db_result>         result;
    ds >> result;
    print("result.size(): ", result.size(), "\n");
    for (auto& x : result) {
        eosio::asset a;
        x.value >> a;
        print(
            "    ", x.block_index, " ", x.present, " ", x.code.to_string(), " ", x.table.to_string(), " ", x.scope.to_string(), " ",
            x.primary_key, " ", x.payer.to_string(), " ", a.amount, "\n");
    }
    print("end wasm\n\n");
}
