# Header file `temp_placeholders.hpp`

``` cpp
extern "C" void print_range(char const* begin, char const* end);

template <typename T>
T& lvalue(T&& v);

template <typename T>
struct serial_wrapper;

template <typename DataStream>
DataStream& operator<<(DataStream& ds, serial_wrapper<eosio::checksum256>& obj);

template <typename DataStream>
DataStream& operator>>(DataStream& ds, serial_wrapper<eosio::checksum256>& obj);

namespace eosio
{
    template <typename Stream>
    datastream<Stream>& operator>>(datastream<Stream>& ds, datastream<Stream>& dest);

    template <typename Stream1, typename Stream2>
    datastream<Stream1>& operator<<(datastream<Stream1>& ds, datastream<Stream2> const& obj);

    template <typename Stream>
    datastream<Stream>& operator>>(datastream<Stream>& ds, std::string_view& dest);

    template <typename Stream>
    datastream<Stream>& operator<<(datastream<Stream>& ds, std::string_view const& obj);
}

std::string_view asset_amount_to_string(eosio::asset const& v);

char const* asset_to_string(eosio::asset const& v);
```
