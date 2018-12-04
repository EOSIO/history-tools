// copyright defined in LICENSE.txt

#include <eosiolib/asset.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/varint.hpp>
#include <memory>
#include <string>
#include <vector>

using namespace eosio;
using namespace std;

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

struct contract_row {
    uint32_t                       block_index = 0;
    bool                           present     = false;
    eosio::name                    code;
    eosio::name                    scope;
    eosio::name                    table;
    uint64_t                       primary_key = 0;
    eosio::name                    payer;
    eosio::datastream<const char*> value{nullptr, 0};
};

struct query_contract_row_range_scope {
    uint32_t    max_block_index = 0;
    eosio::name code;
    eosio::name scope_min;
    eosio::name scope_max;
    eosio::name table;
    uint64_t    primary_key = 0;
    uint32_t    max_results = 1;
};

using query = std::variant<query_contract_row_range_scope>;

extern "C" void exec_query(void* req_begin, void* req_end, void* cb_alloc_data, cb_alloc_fn* cb_alloc);

template <typename Alloc_fn>
inline void exec_query(const query& req, Alloc_fn alloc_fn) {
    auto req_data = eosio::pack(req);
    exec_query(req_data.data(), req_data.data() + req_data.size(), &alloc_fn, [](void* cb_alloc_data, size_t size) -> void* { //
        return (*reinterpret_cast<Alloc_fn*>(cb_alloc_data))(size);
    });
}

inline std::vector<char> exec_query(const query& req) {
    std::vector<char> result;
    exec_query(req, [&result](size_t size) {
        result.resize(size);
        return result.data();
    });
    return result;
}

template <typename result, typename F>
bool for_each_query_result(const std::vector<char>& bytes, F f) {
    eosio::datastream<const char*> ds(bytes.data(), bytes.size());
    unsigned_int                   size;
    ds >> size;
    for (uint32_t i = 0; i < size.value; ++i) {
        result r;
        ds >> r;
        if (!f(r))
            return false;
    }
    return true;
}

template <typename payload, typename F>
bool for_each_contract_row(const std::vector<char>& bytes, F f) {
    return for_each_query_result<contract_row>(bytes, [&](contract_row& row) {
        payload p;
        row.value >> p;
        if (!f(row, p))
            return false;
        return true;
    });
}

extern "C" void startup() {
    print("\nstart wasm\n");
    auto s = exec_query(query_contract_row_range_scope{
        .max_block_index = 30000,
        .code            = eosio::name{"eosio.token"},
        .scope_min       = eosio::name{"eosio"},
        .scope_max       = eosio::name{"eosio.zzzzzz"},
        .table           = eosio::name{"accounts"},
        .primary_key     = 5459781,
        .max_results     = 100,
    });
    for_each_contract_row<asset>(s, [&](contract_row& r, asset& a) {
        print("    ", r.block_index, " ", r.present, " ", r.code, " ", r.table, " ", r.scope, " ", r.primary_key, " ", r.payer);
        if (r.present)
            print(" ", a.amount);
        print("\n");
        return true;
    });
    print("end wasm\n\n");
}
