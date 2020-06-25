## Header file <code>schema.hpp</code></strong>

```
namespace eosio
{
    //=== Get JSON Schema type name (Default) ===//
    template <typename T>
    std::string_view schema_type_name(T*);

    //=== Get JSON Schema type name (Explicit Types) ===//
    std::string_view schema_type_name(uint8_t*);
    std::string_view schema_type_name(int8_t*);
    std::string_view schema_type_name(uint16_t*);
    std::string_view schema_type_name(int16_t*);
    std::string_view schema_type_name(uint32_t*);
    std::string_view schema_type_name(uint64_t*);
    std::string_view schema_type_name(int64_t*);
    std::string_view schema_type_name(eosio::name*);
    std::string_view schema_type_name(eosio::symbol_code*);
    std::string_view schema_type_name(eosio::time_point*);
    std::string_view schema_type_name(eosio::block_timestamp*);
    std::string_view schema_type_name(eosio::extended_asset*);
    std::string_view schema_type_name(eosio::checksum256*);
    std::string_view schema_type_name(shared_memory<datastream<const char *>>*);

    //=== Make JSON Schema (Explicit Types) ===//
    eosio::rope make_json_schema(shared_memory<std::string_view>*);
    eosio::rope make_json_schema(eosio::name*);
    eosio::rope make_json_schema(eosio::symbol_code*);
    eosio::rope make_json_schema(eosio::time_point*);
    eosio::rope make_json_schema(eosio::block_timestamp*);
    eosio::rope make_json_schema(eosio::extended_asset*);
    eosio::rope make_json_schema(eosio::checksum256*);
    eosio::rope make_json_schema(shared_memory<datastream<const char *>>*);
    template <eosio::tagged_variant_options Options, typename ... NamedTypes>
    eosio::rope make_json_schema(tagged_variant<Options, NamedTypes...>*);
    eosio::rope make_json_schema(bool*);
    template <typename T>
    eosio::rope make_json_schema(int*);
    template <typename T>
    eosio::rope make_json_schema(std::optional<T>*);

    //=== Make JSON Schema (Reflected Objects) ===//
    template <typename T>
    eosio::rope make_json_schema(T*);

    //=== Make JSON Schema (Use This) ===//
    template <typename T>
    eosio::rope make_json_schema();
}
```



#### Function <code>eosio::schema_type_name</code></strong>


```
template <typename T>
std::string_view schema_type_name(T*);
```


Get JSON Schema type name. The argument is ignored; it may be `nullptr`.

Returns “”, which prevents the type from being in the `definitions` section of the schema.



---



#### Get JSON Schema type name (Explicit Types)


```
(1) std::string_view schema_type_name(uint8_t*);

(2) std::string_view schema_type_name(int8_t*);

(3) std::string_view schema_type_name(uint16_t*);

(4) std::string_view schema_type_name(int16_t*);

(5) std::string_view schema_type_name(uint32_t*);

(6) std::string_view schema_type_name(uint64_t*);

(7) std::string_view schema_type_name(int64_t*);

(8) std::string_view schema_type_name(eosio::name*);

(9) std::string_view schema_type_name(eosio::symbol_code*);

(10) std::string_view schema_type_name(eosio::time_point*);

(11) std::string_view schema_type_name(eosio::block_timestamp*);

(12) std::string_view schema_type_name(eosio::extended_asset*);

(13) std::string_view schema_type_name(eosio::checksum256*);

(14) std::string_view schema_type_name(shared_memory<datastream<const char *>>*);
```


Get JSON Schema type name. The argument is ignored; it may be `nullptr`.



---



#### Make JSON Schema (Explicit Types)


```
(1) eosio::rope make_json_schema(shared_memory<std::string_view>*);

(2) eosio::rope make_json_schema(eosio::name*);

(3) eosio::rope make_json_schema(eosio::symbol_code*);

(4) eosio::rope make_json_schema(eosio::time_point*);

(5) eosio::rope make_json_schema(eosio::block_timestamp*);

(6) eosio::rope make_json_schema(eosio::extended_asset*);

(7) eosio::rope make_json_schema(eosio::checksum256*);

(8) eosio::rope make_json_schema(shared_memory<datastream<const char *>>*);

(9) template <eosio::tagged_variant_options Options, typename ... NamedTypes>
eosio::rope make_json_schema(tagged_variant<Options, NamedTypes...>*);

(10) eosio::rope make_json_schema(bool*);

(11) template <typename T>
eosio::rope make_json_schema(int*);

(12) template <typename T>
eosio::rope make_json_schema(std::optional<T>*);
```


Convert types to JSON Schema. The argument is ignored; it may be `nullptr`. These overloads handle specified types.



---



#### Function <code>eosio::make_json_schema</code></strong>


```
template <typename T>
eosio::rope make_json_schema(T*);
```


Convert types to JSON Schema. The argument is ignored; it may be `nullptr`.

This overload works with [reflected objects](doc_struct_reflection#standardese-reflection).



---



#### Function <code>eosio::make_json_schema</code></strong>


```
template <typename T>
eosio::rope make_json_schema();
```


Convert types to JSON Schema and return result. This overload creates a schema including the `definitions` section. The other overloads assume the definitions already exist.
