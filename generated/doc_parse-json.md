# Header file `parse-json.hpp`

``` cpp
namespace eosio
{
    //=== Parse JSON (Explicit Types) ===//
    void parse_json(std::string_view& result, char const*& pos, char const* end);
    void parse_json(uint32_t& result, char const*& pos, char const* end);
    void parse_json(int32_t& result, char const*& pos, char const* end);
    void parse_json(bool& result, char const*& pos, char const* end);
    void parse_json(eosio::name& result, char const*& pos, char const* end);
    void parse_json(eosio::symbol_code& result, char const*& pos, char const* end);
    void parse_json(serial_wrapper<eosio::checksum256>& result, char const*& pos, char const* end);
    template <typename T>
    void parse_json(std::vector<T>& result, char const*& pos, char const* end);
    template <eosio::tagged_variant_options Options, typename ... NamedTypes>
    void parse_json(tagged_variant<Options, NamedTypes...>& result, char const*& pos, char const* end);

    //=== Parse JSON (Reflected Objects) ===//
    template <typename T>
    void parse_json(T& result, char const*& pos, char const* end);

    //=== Convenience Wrappers ===//
    template <typename T>
    T parse_json(std::vector<char> const& v);

    template <typename T>
    T parse_json(std::string_view s);

    //=== JSON Conversion Helpers ===//
    void parse_json_skip_space(char const*& pos, char const* end);

    void parse_json_skip_value(char const*& pos, char const* end);

    void parse_json_expect(char const*& pos, char const* end, char ch, char const* msg);

    void parse_json_expect_end(char const*& pos, char const* end);
}
```

### Parse JSON (Explicit Types)

``` cpp
(1) void parse_json(std::string_view& result, char const*& pos, char const* end);

(2) void parse_json(uint32_t& result, char const*& pos, char const* end);

(3) void parse_json(int32_t& result, char const*& pos, char const* end);

(4) void parse_json(bool& result, char const*& pos, char const* end);

(5) void parse_json(eosio::name& result, char const*& pos, char const* end);

(6) void parse_json(eosio::symbol_code& result, char const*& pos, char const* end);

(7) void parse_json(serial_wrapper<eosio::checksum256>& result, char const*& pos, char const* end);

(8) template <typename T>
void parse_json(std::vector<T>& result, char const*& pos, char const* end);

(9) template <eosio::tagged_variant_options Options, typename ... NamedTypes>
void parse_json(tagged_variant<Options, NamedTypes...>& result, char const*& pos, char const* end);
```

Parse JSON and convert to `result`. These overloads handle specified types.

-----

### Function `eosio::parse_json`

``` cpp
template <typename T>
void parse_json(T& result, char const*& pos, char const* end);
```

Parse JSON and convert to `result`. This overload works with [reflected objects](doc_struct-reflection.md#standardese-reflection).

-----

### Function `eosio::parse_json`

``` cpp
template <typename T>
T parse_json(std::vector<char> const& v);
```

Parse JSON and return result. This overload wraps the other `to_json` overloads.

-----

### Function `eosio::parse_json`

``` cpp
template <typename T>
T parse_json(std::string_view s);
```

Parse JSON and return result. This overload wraps the other `to_json` overloads.

-----

### Function `eosio::parse_json_skip_space`

``` cpp
void parse_json_skip_space(char const*& pos, char const* end);
```

Skip spaces

-----

### Function `eosio::parse_json_skip_value`

``` cpp
void parse_json_skip_value(char const*& pos, char const* end);
```

Skip a JSON value. Caution: only partially implemented; currently mishandles most cases.

-----

### Function `eosio::parse_json_expect`

``` cpp
void parse_json_expect(char const*& pos, char const* end, char ch, char const* msg);
```

Asserts `ch` is next character. `msg` is the assertion message.

-----

### Function `eosio::parse_json_expect_end`

``` cpp
void parse_json_expect_end(char const*& pos, char const* end);
```

Asserts `pos == end`.

-----
