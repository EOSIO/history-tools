// copyright defined in LICENSE.txt

// todo: remove or replace everything in this file

#include <eosio/temp_placeholders.hpp>

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

namespace internal_use_do_not_use {

extern "C" void eosio_assert(uint32_t test, const char* msg) {
    if (!test)
        eosio_assert_message(test, msg, strlen(msg));
}

} // namespace internal_use_do_not_use
} // namespace eosio
