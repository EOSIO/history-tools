## Header file <code>block_select.hpp</code></strong>


```
using absolute_block     = tagged_type<"absolute"_n, int32_t>;
using head_block         = tagged_type<"head"_n, int32_t>;
using irreversible_block = tagged_type<"irreversible"_n, int32_t>;

using block_select       = tagged_variant<
                               serialize_tag_as_index,
                               absolute_block,
                               head_block,
                               irreversible_block>;

block_select make_absolute_block(int32_t i);

uint32_t get_block_num(const block_select& sel, const database_status& status);
```


`block_select` identifies blocks. This appears in JSON as one of the following:


```
["absolute", 1234]          Block 1234
["head", -10]               10 blocks behind head
["irreversible", -4]        4 blocks behind irreversible

block_select make_absolute_block(int32_t i);
```


Returns a `block_select` which references block `i`


```
uint32_t get_block_num(const block_select& sel, const database_status& status);
```


Returns the block that `sel` references.
