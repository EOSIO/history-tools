# Header file `temp-placeholders.cpp`

``` cpp
extern "C" void* memcpy(void* dest, void const* src, size_t size);

extern "C" void* memmove(void* dest, void const* src, size_t size);

extern "C" void* memset(void* dest, int v, size_t size);

extern "C" void prints(char const* cstr);

extern "C" void prints_l(char const* cstr, uint32_t len);

extern "C" void printn(uint64_t n);

extern "C" void printui(uint64_t value);

extern "C" void printi(int64_t value);

namespace eosio
{
    void print(std::string_view sv);

    namespace internal_use_do_not_use
    {
        extern "C" void eosio_assert(uint32_t test, char const* msg);
    }
}
```
<<(datastream<Stream1>& ds, datastream<Stream2> const& obj);

    template <typename Stream>
    datastream<Stream>& operator>>(datastream<Stream>& ds, std::string_view& dest);

    template <typename Stream>
    datastream<Stream>& operator<<(datastream<Stream>& ds, std::string_view const& obj);
}

std::string_view asset_amount_to_string(eosio::asset const& v);

char const* asset_to_string(eosio::asset const& v);
```
