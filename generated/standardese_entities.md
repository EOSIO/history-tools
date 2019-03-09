# Project index

  - [`STRUCT_MEMBER`](doc_struct_reflection.md#standardese-reflection)

  - [`STRUCT_REFLECT`](doc_struct_reflection.md#standardese-reflection)

  - [`asset_amount_to_string`](doc_temp_placeholders.md#standardese-temp_placeholders-hpp)

  - [`asset_to_string`](doc_temp_placeholders.md#standardese-temp_placeholders-hpp)

  - ## Namespace `eosio`
    
      - [`account`](doc_database.md#standardese-eosio__account) - Details about an account
    
      - [`action_trace`](doc_database.md#standardese-eosio__action_trace) - Details about action execution
    
      - [`block_info`](doc_database.md#standardese-eosio__block_info) - Information extracted from a block
    
      - [`contract_row`](doc_database.md#standardese-eosio__contract_row) - A row in a contract’s table
    
      - [`contract_secondary_index_with_row`](doc_database.md#standardese-eosio__contract_secondary_index_with_row-T-) - A secondary index entry in a contract’s table. Also includes fields from `contract_row`.
    
      - [`database_status`](doc_database.md#standardese-eosio__database_status) - Status of the database. Returned by `get_database_status`.
    
      - [`for_each_contract_row`](doc_database.md#standardese-eosio__for_each_contract_row-T-F--std__vector-char-const--F-) - Use with `query_contract_row_*`. Unpack each row of a query result and call `f(row, data)`. `row` is an instance of `contract_row`. `data` is the unpacked contract-specific data. `T` identifies the type of `data`.
    
      - [`for_each_query_result`](doc_database.md#standardese-eosio__for_each_query_result-T-F--std__vector-char-const--F-) - Unpack each record of a query result and call `f(record)`. `T` is the record type.
    
      - [`get_database_status`](doc_database.md#standardese-eosio)
    
      - [`get_input_data`](doc_input_output.md#standardese-eosio)
    
      - [`increment_key`](doc_database.md#standardese-eosio__increment_key-uint8_t--) - Increment a database key. Return true if the result wrapped.
    
      - ### Namespace `eosio::internal_use_do_not_use`
        
          - [`eosio_assert`](doc_temp_placeholders.md#standardese-eosio__internal_use_do_not_use)
    
      - [`kv_to_json`](doc_to_json.md#standardese-eosio__kv_to_json-T--std__string_view-Tconst--)
    
      - [`make_json_schema`](doc_schema.md#standardese-eosio__make_json_schema-std__string_view--) - Convert types to JSON Schema. Appends to `dest`. The first argument is ignored; it may be `nullptr`. These overloads handle specified types.
    
      - [`operator<<`](doc_temp_placeholders.md#standardese-eosio)
    
      - [`operator>>`](doc_temp_placeholders.md#standardese-eosio)
    
      - [`parse_json`](doc_parse_json.md#standardese-eosio__parse_json-std__string_view--charconst---charconst--) - Parse JSON and convert to `result`. These overloads handle specified types.
    
      - [`parse_json_expect`](doc_parse_json.md#standardese-eosio__parse_json_expect-charconst---charconst--char-charconst--) - Asserts `ch` is next character. `msg` is the assertion message.
    
      - [`parse_json_expect_end`](doc_parse_json.md#standardese-eosio__parse_json_expect_end-charconst---charconst--) - Asserts `pos == end`.
    
      - [`parse_json_skip_space`](doc_parse_json.md#standardese-eosio__parse_json_skip_space-charconst---charconst--) - Skip spaces
    
      - [`parse_json_skip_value`](doc_parse_json.md#standardese-eosio__parse_json_skip_value-charconst---charconst--) - Skip a JSON value. Caution: only partially implemented; currently mishandles most cases.
    
      - [`print`](doc_temp_placeholders.md#standardese-eosio)
    
      - [`query_account_range_name`](doc_database.md#standardese-eosio__query_account_range_name) - Pass this to `query_database` to get `account` for a range of names.
    
      - [`query_action_trace_executed_range_name_receiver_account_block_trans_action`](doc_database.md#standardese-eosio__query_action_trace_executed_range_name_receiver_account_block_trans_action) - Pass this to `query_database` to get `action_trace` for a range of keys. Only includes actions in executed transactions.
    
      - [`query_block_info_range_index`](doc_database.md#standardese-eosio__query_block_info_range_index) - Pass this to `query_database` to get `block_info` for a range of block indexes.
    
      - [`query_contract_index64_range_code_table_scope_sk_pk`](doc_database.md#standardese-eosio__query_contract_index64_range_code_table_scope_sk_pk) - Pass this to `query_database` to get `contract_secondary_index_with_row<uint64_t>` for a range of keys.
    
      - [`query_contract_row_range_code_table_pk_scope`](doc_database.md#standardese-eosio__query_contract_row_range_code_table_pk_scope) - Pass this to `query_database` to get `contract_row` for a range of keys.
    
      - [`query_contract_row_range_code_table_scope_pk`](doc_database.md#standardese-eosio__query_contract_row_range_code_table_scope_pk) - Pass this to `query_database` to get `contract_row` for a range of keys.
    
      - [`query_contract_row_range_scope_table_pk_code`](doc_database.md#standardese-eosio__query_contract_row_range_scope_table_pk_code) - Pass this to `query_database` to get `contract_row` for a range of keys.
    
      - [`query_database`](doc_database.md#standardese-eosio__query_database-T--Tconst--) - Query the database. `request` must be one of the `query_*` structs. Returns result in serialized form.
    
      - [`schema_type_name`](doc_schema.md#standardese-eosio__schema_type_name-T--T--) - Get JSON Schema type name. The first argument is ignored; it may be `nullptr`.
    
      - [`set_output_data`](doc_input_output.md#standardese-set_output_data-charconst--charconst--) - Set the wasm’s output data
    
      - [`tagged_type`](doc_tagged_variant.md#standardese-eosio__tagged_type-N-T-) - Pairs a name with a type
    
      - [`tagged_variant`](doc_tagged_variant.md#standardese-eosio__tagged_variant-Options-NamedTypes-) - Type-safe union
    
      - [`tagged_variant_options`](doc_tagged_variant.md#standardese-eosio__tagged_variant_options) - Options for `tagged_variant`
    
      - [`to_json`](doc_to_json.md#standardese-eosio__to_json-std__string_view-) - Convert objects to JSON. These overloads handle specified types.
    
      - [`transaction_status`](doc_database.md#standardese-eosio__transaction_status) - Transaction status

  - [`lvalue`](doc_temp_placeholders.md#standardese-temp_placeholders-hpp)

  - [`memcpy`](doc_temp_placeholders.md#standardese-temp_placeholders-cpp)

  - [`memmove`](doc_temp_placeholders.md#standardese-temp_placeholders-cpp)

  - [`memset`](doc_temp_placeholders.md#standardese-temp_placeholders-cpp)

  - [`operator<<`](doc_temp_placeholders.md#standardese-temp_placeholders-hpp)

  - [`operator>>`](doc_temp_placeholders.md#standardese-temp_placeholders-hpp)

  - [`print_range`](doc_temp_placeholders.md#standardese-temp_placeholders-hpp)

  - [`printi`](doc_temp_placeholders.md#standardese-temp_placeholders-cpp)

  - [`printn`](doc_temp_placeholders.md#standardese-temp_placeholders-cpp)

  - [`prints`](doc_temp_placeholders.md#standardese-temp_placeholders-cpp)

  - [`prints_l`](doc_temp_placeholders.md#standardese-temp_placeholders-cpp)

  - [`printui`](doc_temp_placeholders.md#standardese-temp_placeholders-cpp)

  - [`serial_wrapper`](doc_temp_placeholders.md#standardese-temp_placeholders-hpp)
