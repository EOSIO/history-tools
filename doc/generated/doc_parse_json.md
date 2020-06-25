## Header file <code>parse_json.hpp</code></strong>


```
namespace eosio
{
    //=== Parse JSON (Explicit Types) ===//
    void parse_json(std::string_view& result, char const*& pos, char const* end);
    void parse_json(shared_memory<std::string_view>& result, char const*& pos, char const* end);
    void parse_json(uint8_t& result, char const*& pos, char const* end);
    void parse_json(uint16_t& result, char const*& pos, char const* end);
    void parse_json(uint32_t& result, char const*& pos, char const* end);
    void parse_json(uint64_t& result, char const*& pos, char const* end);
    void parse_json(bool& result, char const*& pos, char const* end);
    void parse_json(eosio::name& result, char const*& pos, char const* end);
    void parse_json(eosio::symbol_code& result, char const*& pos, char const* end);
    void parse_json(eosio::checksum256& result, char const*& pos, char const* end);
    template <typename T>
    void parse_json(int& result, char const*& pos, char const* end);
    template <eosio::tagged_variant_options Options, typename ... NamedTypes>
    void parse_json(tagged_variant<Options, NamedTypes...>& result, char const*& pos, char const* end);

    //=== Parse JSON (Reflected Objects) ===//
    template <typename T>
    void parse_json(T& result, char const*& pos, char const* end);

    //=== Convenience Wrappers ===//
    template <typename T>
    T parse_json(int const& v);

    template <typename T>
    T parse_json(std::string_view s);

    //=== JSON Conversion Helpers ===//
    void parse_json_skip_space(char const*& pos, char const* end);

    void parse_json_skip_value(char const*& pos, char const* end);

    void parse_json_expect(char const*& pos, char const* end, char ch, char const* msg);

    void parse_json_expect_end(char const*& pos, char const* end);
}
```



#### Parse JSON (Explicit Types)


```
(1) void parse_json(std::string_view& result, char const*& pos, char const* end);

(2) void parse_json(shared_memory<std::string_view>& result, char const*& pos, char const* end);

(3) void parse_json(uint8_t& result, char const*& pos, char const* end);

(4) void parse_json(uint16_t& result, char const*& pos, char const* end);

(5) void parse_json(uint32_t& result, char const*& pos, char const* end);

(6) void parse_json(uint64_t& result, char const*& pos, char const* end);

(7) void parse_json(bool& result, char const*& pos, char const* end);

(8) void parse_json(eosio::name& result, char const*& pos, char const* end);

(9) void parse_json(eosio::symbol_code& result, char const*& pos, char const* end);

(10) void parse_json(eosio::checksum256& result, char const*& pos, char const* end);

(11) template <typename T>
void parse_json(int& result, char const*& pos, char const* end);

(12) template <eosio::tagged_variant_options Options, typename ... NamedTypes>
void parse_json(tagged_variant<Options, NamedTypes...>& result, char const*& pos, char const* end);
```


Parse JSON and convert to `result`. These overloads handle specified types.



---



#### Function <code>eosio::parse_json</code></strong>


```
template <typename T>
void parse_json(T& result, char const*& pos, char const* end);
```


Parse JSON and convert to `result`. This overload works with [reflected objects](./doc_struct_reflection#standardese-reflection).



---



#### Function <code>eosio::parse_json</code></strong>


```
template <typename T>
T parse_json(int const& v);
```


Parse JSON and return result. This overload wraps the other `to_json` overloads.



---



#### Function <code>eosio::parse_json</code></strong>


```
template <typename T>
T parse_json(std::string_view s);
```


Parse JSON and return result. This overload wraps the other `to_json` overloads.



---



#### Function <code>eosio::parse_json_skip_space</code></strong>


```
void parse_json_skip_space(char const*& pos, char const* end);
```


Skip spaces



---



#### Function <code>eosio::parse_json_skip_value</code></strong>


```
void parse_json_skip_value(char const*& pos, char const* end);
```


Skip a JSON value. Caution: only partially implemented; currently mishandles most cases.



---



#### Function <code>eosio::parse_json_expect</code></strong>


```
void parse_json_expect(char const*& pos, char const* end, char ch, char const* msg);
```


Asserts `ch` is next character. `msg` is the assertion message.



---



#### Function <code>eosio::parse_json_expect_end</code></strong>


```
void parse_json_expect_end(char const*& pos, char const* end);
```


Asserts `pos == end`.



---

