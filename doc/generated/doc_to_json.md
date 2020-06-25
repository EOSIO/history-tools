## Header file <code>to_json.hpp</code></strong>


```
namespace eosio
{
    //=== Convert explicit types to JSON ===//
    eosio::rope to_json(std::string_view sv);
    eosio::rope to_json(std::string s);
    eosio::rope to_json(shared_memory<std::string_view> sv);
    eosio::rope to_json(bool value);
    eosio::rope to_json(uint8_t value);
    eosio::rope to_json(uint16_t value);
    eosio::rope to_json(uint32_t value);
    eosio::rope to_json(uint64_t value);
    eosio::rope to_json(eosio::unsigned_int value);
    eosio::rope to_json(int8_t value);
    eosio::rope to_json(int16_t value);
    eosio::rope to_json(int64_t value);
    eosio::rope to_json(eosio::signed_int value);
    eosio::rope to_json(double value);
    eosio::rope to_json(float value);
    eosio::rope to_json(eosio::name value);
    eosio::rope to_json(eosio::symbol_code value);
    eosio::rope to_json(eosio::asset value);
    eosio::rope to_json(eosio::extended_asset value);
    eosio::rope to_json(eosio::checksum256 const& value);
    eosio::rope to_json(eosio::time_point value);
    eosio::rope to_json(eosio::block_timestamp value);
    eosio::rope to_json(shared_memory<datastream<const char *>> const& value);
    template <typename T>
    eosio::rope to_json(std::optional<T> const& obj);
    template <typename T>
    eosio::rope to_json(int const& obj);
    eosio::rope to_json(int const& obj);
    template <eosio::tagged_variant_options Options, typename ... NamedTypes>
    eosio::rope to_json(tagged_variant<Options, NamedTypes...> const& v);

    //=== Convert reflected objects to JSON ===//
    template <typename T>
    eosio::rope to_json(T const& obj);
}
```



#### Convert explicit types to JSON


```
(1) eosio::rope to_json(std::string_view sv);

(2) eosio::rope to_json(std::string s);

(3) eosio::rope to_json(shared_memory<std::string_view> sv);

(4) eosio::rope to_json(bool value);

(5) eosio::rope to_json(uint8_t value);

(6) eosio::rope to_json(uint16_t value);

(7) eosio::rope to_json(uint32_t value);

(8) eosio::rope to_json(uint64_t value);

(9) eosio::rope to_json(eosio::unsigned_int value);

(10) eosio::rope to_json(int8_t value);

(11) eosio::rope to_json(int16_t value);

(12) eosio::rope to_json(int64_t value);

(13) eosio::rope to_json(eosio::signed_int value);

(14) eosio::rope to_json(double value);

(15) eosio::rope to_json(float value);

(16) eosio::rope to_json(eosio::name value);

(17) eosio::rope to_json(eosio::symbol_code value);

(18) eosio::rope to_json(eosio::asset value);

(19) eosio::rope to_json(eosio::extended_asset value);

(20) eosio::rope to_json(eosio::checksum256 const& value);

(21) eosio::rope to_json(eosio::time_point value);

(22) eosio::rope to_json(eosio::block_timestamp value);

(23) eosio::rope to_json(shared_memory<datastream<const char *>> const& value);

(24) template <typename T>
eosio::rope to_json(std::optional<T> const& obj);

(25) template <typename T>
eosio::rope to_json(int const& obj);

(26) eosio::rope to_json(int const& obj);

(27) template <eosio::tagged_variant_options Options, typename ... NamedTypes>
eosio::rope to_json(tagged_variant<Options, NamedTypes...> const& v);
```


Convert objects to JSON. These overloads handle specified types.



---



#### Function <code>eosio::to_json</code></strong>


```
template <typename T>
eosio::rope to_json(T const& obj);
```


Convert an object to JSON. This overload works with [reflected objects](./doc_struct_reflection#standardese-reflection).



---

