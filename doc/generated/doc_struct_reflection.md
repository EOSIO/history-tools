## Header file <code>struct_reflection.hpp</code></strong>


```
#define STRUCT_REFLECT(STRUCT)

#define STRUCT_MEMBER(STRUCT, MEMBER)
```


These macros describe a struct to the JSON conversion and JSON schema functions.

Members mentioned by `STRUCT_MEMBER` are converted. Members not mentioned are not converted.

Example use:


```
namespace my_namespace {

    struct token_balance {
        eosio::name           account = {};
        eosio::extended_asset amount  = {};
    };

    STRUCT_REFLECT(my_namespace::token_balance) {
        STRUCT_MEMBER(token_balance, account)
        STRUCT_MEMBER(token_balance, amount)
    }

}
