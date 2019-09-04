# Header file `temp_placeholders.cpp`

``` cpp
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
