# Header file `to-json.hpp`

``` cpp
namespace eosio
{
    //=== Convert explicit types to JSON ===//
    void to_json(std::string_view sv, std::vector<char>& dest);
    void to_json(bool value, std::vector<char>& dest);
    void to_json(uint8_t value, std::vector<char>& dest);
    void to_json(uint32_t value, std::vector<char>& dest);
    void to_json(uint32_t value, int digits, std::vector<char>& dest);
    void to_json(uint16_t value, std::vector<char>& dest);
    void to_json(int32_t value, std::vector<char>& dest);
    void to_json(int64_t value, std::vector<char>& dest);
    void to_json(eosio::name value, std::vector<char>& dest);
    void to_json(eosio::symbol_code value, std::vector<char>& dest);
    void to_json(eosio::asset value, std::vector<char>& dest);
    void to_json(eosio::extended_asset value, std::vector<char>& dest);
    void to_json(serial_wrapper<eosio::checksum256> const& value, std::vector<char>& dest);
    void to_json(eosio::time_point value, std::vector<char>& dest);
    void to_json(eosio::block_timestamp value, std::vector<char>& dest);
    void to_json(datastream<const char *> const& value, std::vector<char>& dest);
    template <typename T>
    void to_json(std::optional<T> const& obj, std::vector<char>& dest);
    template <typename T>
    void to_json(std::vector<T> const& obj, std::vector<char>& dest);
    template <eosio::tagged_variant_options Options, typename ... NamedTypes>
    void to_json(tagged_variant<Options, NamedTypes...> const& v, std::vector<char>& dest);

    //=== Convert reflected objects to JSON ===//
    template <typename T>
    void to_json(T const& obj, std::vector<char>& dest);

    //=== Convenience Wrapper ===//
    template <typename T>
    std::vector<char> to_json(T const& obj);

    //=== JSON Conversion Helpers ===//
    void append(std::vector<char>& dest, std::string_view sv);

    template <typename T>
    void kv_to_json(std::string_view key, T const& value, std::vector<char>& dest);
}
```

### Convert explicit types to JSON

``` cpp
(1) void to_json(std::string_view sv, std::vector<char>& dest);

(2) void to_json(bool value, std::vector<char>& dest);

(3) void to_json(uint8_t value, std::vector<char>& dest);

(4) void to_json(uint32_t value, std::vector<char>& dest);

(5) void to_json(uint32_t value, int digits, std::vector<char>& dest);

(6) void to_json(uint16_t value, std::vector<char>& dest);

(7) void to_json(int32_t value, std::vector<char>& dest);

(8) void to_json(int64_t value, std::vector<char>& dest);

(9) void to_json(eosio::name value, std::vector<char>& dest);

(10) void to_json(eosio::symbol_code value, std::vector<char>& dest);

(11) void to_json(eosio::asset value, std::vector<char>& dest);

(12) void to_json(eosio::extended_asset value, std::vector<char>& dest);

(13) void to_json(serial_wrapper<eosio::checksum256> const& value, std::vector<char>& dest);

(14) void to_json(eosio::time_point value, std::vector<char>& dest);

(15) void to_json(eosio::block_timestamp value, std::vector<char>& dest);

(16) void to_json(datastream<const char *> const& value, std::vector<char>& dest);

(17) template <typename T>
void to_json(std::optional<T> const& obj, std::vector<char>& dest);

(18) template <typename T>
void to_json(std::vector<T> const& obj, std::vector<char>& dest);

(19) template <eosio::tagged_variant_options Options, typename ... NamedTypes>
void to_json(tagged_variant<Options, NamedTypes...> const& v, std::vector<char>& dest);
```

Convert objects to JSON. Appends to `dest`. These overloads handle specified types.

-----

### Function `eosio::to_json`

``` cpp
template <typename T>
void to_json(T const& obj, std::vector<char>& dest);
```

Convert an object to JSON. Appends to `dest`. This overload works with [reflected objects](doc_struct-reflection.md#standardese-reflection).

-----

### Function `eosio::to_json`

``` cpp
template <typename T>
std::vector<char> to_json(T const& obj);
```

Convert an object to JSON. This overload wraps the other `to_json` overloads. Returns result.

-----

### Function `eosio::append`

``` cpp
void append(std::vector<char>& dest, std::string_view sv);
```

Append content in `sv` to `dest`

-----
