# Header file `tagged-variant.hpp`

``` cpp
namespace eosio
{
    template <name::raw N, typename T>
    struct tagged_type;

    enum tagged_variant_options;

    template <eosio::tagged_variant_options Options, typename ... NamedTypes>
    struct tagged_variant;
}
```

A `tagged_variant` is a type-safe union; it may contain one value from a set of types. Each type is tagged with a name; this name appears in the JSON representation and optionally in the binary serialization.

Example:

``` c++
using token_query_request = eosio::tagged_variant<                                //
    eosio::serialize_tag_as_name,                                                 //
    eosio::tagged_type<"transfer"_n, token_transfer_request>,                     //
    eosio::tagged_type<"bal.mult.acc"_n, balances_for_multiple_accounts_request>, //
    eosio::tagged_type<"bal.mult.tok"_n, balances_for_multiple_tokens_request>>;  //
```

The JSON representation takes one of the following forms:

    ["transfer", { ...token_transfer_request properties... }]
    ["bal.mult.acc", { ...balances_for_multiple_accounts_request properties... }]
    ["bal.mult.tok", { ...balances_for_multiple_tokens_request properties... }]

Since this example uses the `eosio::serialize_tag_as_name` flag, the binary serialization for instances of this type contains:

  - 8-byte `eosio::name` containing “transfer”, “bal.mult.acc”, or “bal.mult.tok”

  - bytes for `token_transfer_request`, `balances_for_multiple_accounts_request`, or balances\_for\_multiple\_tokens\_request

If the `serialize_tag_as_index` was used instead, the binary serialization would contain:

  - 1-byte `eosio::unsigned_int` containing 0, 1, or 2

  - bytes for `token_transfer_request`, `balances_for_multiple_accounts_request`, or balances\_for\_multiple\_tokens\_request

The 1-byte `eosio::unsigned_int` format is more compact, and is compatible with ABI 1.1’s variant serialization. The 8-byte `name` format provides an easier path to upgrading the format of binary data in the future.

### Struct `eosio::tagged_type`

``` cpp
template <name::raw N, typename T>
struct tagged_type
{
    static constexpr eosio::name const name = eosio::name{N};

    using type = T;
};
```

Pairs a name with a type

-----

### Enumeration `eosio::tagged_variant_options`

``` cpp
enum tagged_variant_options
{
    serialize_tag_as_name = 1,
    serialize_tag_as_index = 2
};
```

Options for `tagged_variant`

-----

### Struct `eosio::tagged_variant`

``` cpp
template <eosio::tagged_variant_options Options, typename ... NamedTypes>
struct tagged_variant
{
    std::variant<typename NamedTypes::type...> value;

    static constexpr eosio::name const keys[] = {NamedTypes::name...};
};
```

Type-safe union

-----
