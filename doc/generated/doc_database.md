## Header file <code>database.hpp</code></strong>


```
namespace eosio
{
    //=== Increment Key ===//
    bool increment_key(uint8_t& key);
    bool increment_key(uint16_t& key);
    bool increment_key(uint32_t& key);
    bool increment_key(uint64_t& key);
    bool increment_key(uint128_t& key);
    bool increment_key(int& key);
    bool increment_key(eosio::checksum256& key);
    bool increment_key(query_action_trace_range_name_receiver_account_block_trans_action::key& key);
    bool increment_key(query_action_trace_receipt_receiver::key& key);
    bool increment_key(query_transaction_receipt::key& key);
    bool increment_key(query_contract_row_range_code_table_pk_scope::key& key);
    bool increment_key(query_contract_row_range_code_table_scope_pk::key& key);
    bool increment_key(query_contract_row_range_scope_table_pk_code::key& key);
    bool increment_key(query_contract_index64_range_code_table_scope_sk_pk::key& key);

    //=== Tables ===//
    enum class transaction_status
    : uint8_t;

    //=== to_json_explicit ===//
    int to_json(eosio::transaction_status value);

    struct block_info;

    std::string_view schema_type_name(eosio::block_info*);

    template <typename F>
    void for_each_member(eosio::block_info*, F f);

    struct receipt;

    std::string_view schema_type_name(eosio::receipt*);

    template <typename F>
    void for_each_member(eosio::receipt*, F f);

    struct action;

    std::string_view schema_type_name(eosio::action*);

    template <typename F>
    void for_each_member(eosio::action*, F f);

    struct action_trace;

    std::string_view schema_type_name(eosio::action_trace*);

    template <typename F>
    void for_each_member(eosio::action_trace*, F f);

    struct account;

    std::string_view schema_type_name(eosio::account*);

    template <typename F>
    void for_each_member(eosio::account*, F f);

    struct code_key;

    struct account_metadata_joined;

    struct metadata_code_joined;

    struct contract_row;

    template <typename T>
    struct contract_secondary_index_with_row;

    //=== Queries ===//
    struct query_block_info_range_index;

    struct query_action_trace_range_name_receiver_account_block_trans_action;

    struct query_action_trace_receipt_receiver;

    struct query_transaction_receipt;

    struct query_account_range_name;

    struct query_acctmeta_range_name;

    struct query_code_range_name;

    struct query_contract_row_range_code_table_pk_scope;

    struct query_contract_row_range_code_table_scope_pk;

    struct query_contract_row_range_scope_table_pk_code;

    struct query_contract_index64_range_code_table_scope_sk_pk;

    //=== Database Status ===//
    struct database_status;

    template <typename F>
    void for_each_member(eosio::database_status*, F f);

    eosio::database_status get_database_status();

    //=== Query Database ===//
    template <typename T>
    int query_database();

    template <typename T, typename F>
    bool for_each_query_result(int const& bytes, F f);

    template <typename T, typename F>
    bool for_each_contract_row(int const& bytes, F f);
}
```



#### Increment Key


```
(1) bool increment_key(uint8_t& key);

(2) bool increment_key(uint16_t& key);

(3) bool increment_key(uint32_t& key);

(4) bool increment_key(uint64_t& key);

(5) bool increment_key(uint128_t& key);

(6) bool increment_key(int& key);

(7) bool increment_key(eosio::checksum256& key);

(8) bool increment_key(query_action_trace_range_name_receiver_account_block_trans_action::key& key);

(9) bool increment_key(query_action_trace_receipt_receiver::key& key);

(10) bool increment_key(query_transaction_receipt::key& key);

(11) bool increment_key(query_contract_row_range_code_table_pk_scope::key& key);

(12) bool increment_key(query_contract_row_range_code_table_scope_pk::key& key);

(13) bool increment_key(query_contract_row_range_scope_table_pk_code::key& key);

(14) bool increment_key(query_contract_index64_range_code_table_scope_sk_pk::key& key);
```


Increment a database key. Return true if the result wrapped.



---



#### Enumeration <code>eosio::transaction_status</code></strong>


```
enum class transaction_status
: uint8_t
{
    executed = 0,
    soft_fail = 1,
    hard_fail = 2,
    delayed = 3,
    expired = 4
};
```


Transaction status


##### Enumerators



*   `executed` - succeed, no error handler executed
*   `soft_fail` - objectively failed (not executed), error handler executed
*   `hard_fail` - objectively failed and error handler objectively failed thus no state change
*   `delayed` - transaction delayed/deferred/scheduled for future execution
*   `expired` - transaction expired and storage space refunded to user



---



#### Function <code>eosio::to_json</code></strong>


```
(1) int to_json(eosio::transaction_status value);
```




---



#### Struct <code>eosio::block_info</code></strong>


```
struct block_info
{
    uint32_t block_num = {};

    eosio::checksum256 block_id = {};

    int timestamp = block_timestamp{};

    int producer = {};

    uint16_t confirmed = {};

    eosio::checksum256 previous = {};

    eosio::checksum256 transaction_mroot = {};

    eosio::checksum256 action_mroot = {};

    uint32_t schedule_version = {};

    uint32_t new_producers_version = {};
};
```


Information extracted from a block



---



#### Struct <code>eosio::receipt</code></strong>


```
struct receipt
{
    int receiver = {};

    eosio::checksum256 act_digest = {};

    uint64_t global_sequence = {};

    uint64_t recv_sequence = {};

    eosio::unsigned_int code_sequence = {};

    eosio::unsigned_int abi_sequence = {};

    int EOSLIB_SERIALIZE(eosio::receipt, int receiver(int)(int)(int)(int)(int));
};
```


Details about action execution



---



#### Struct <code>eosio::action</code></strong>


```
struct action
{
    int account = {};

    int name = {};

    shared_memory<datastream<const char *>> data = {};

    int EOSLIB_SERIALIZE(eosio::action, int account(int)(int));
};
```


Details about action execution



---



#### Struct <code>eosio::action_trace</code></strong>


```
struct action_trace
{
    uint32_t block_num = {};

    eosio::checksum256 transaction_id = {};

    eosio::transaction_status transaction_status = {};

    eosio::unsigned_int action_ordinal = {};

    eosio::unsigned_int creator_action_ordinal = {};

    std::optional<receipt> receipt = {};

    int receiver = {};

    action action = {};

    bool context_free = {};

    int64_t elapsed = {};

    std::string console = {};

    std::string except = {};

    uint64_t error_code = {};

    int EOSLIB_SERIALIZE(eosio::action_trace, int block_num(int)(enum transaction_status)(int)(int)(receipt)(int)(action)(int)(int)(int)(int)(int));
};
```


Details about action execution



---



#### Struct <code>eosio::account</code></strong>


```
struct account
{
    uint32_t block_num = {};

    bool present = {};

    int name = {};

    int creation_date = block_timestamp_type{};

    shared_memory<datastream<const char *>> abi = {};

    int EOSLIB_SERIALIZE(eosio::account, int block_num(int)(int)(int)(int));
};
```


Details about an account



---



#### Struct <code>eosio::code_key</code></strong>


```
struct code_key
{
    uint8_t vm_type = {};

    uint8_t vm_version = {};

    eosio::checksum256 hash = {};

    int EOSLIB_SERIALIZE(eosio::code_key, int vm_type(int)(int));
};
```


Key for looking up code



---



#### Struct <code>eosio::account_metadata_joined</code></strong>


```
struct account_metadata_joined
{
    uint32_t block_num = {};

    bool present = {};

    int name = {};

    bool privileged = {};

    int last_code_update = time_point{};

    std::optional<code_key> code = {};

    uint32_t account_block_num = {};

    bool account_present = {};

    int account_creation_date = block_timestamp_type{};

    shared_memory<datastream<const char *>> account_abi = {};

    int EOSLIB_SERIALIZE(eosio::account_metadata_joined, int block_num(int)(int)(int)(int)(int)(int)(int)(int)(int));
};
```


account and account_metadata joined



---



#### Struct <code>eosio::metadata_code_joined</code></strong>


```
struct metadata_code_joined
{
    uint32_t block_num = {};

    bool present = {};

    int name = {};

    bool privileged = {};

    int last_code_update = time_point{};

    std::optional<code_key> code = {};

    uint32_t join_block_num = {};

    bool join_present = {};

    uint8_t join_vm_type = {};

    uint8_t join_vm_version = {};

    eosio::checksum256 join_code_hash = {};

    shared_memory<datastream<const char *>> join_code = {};

    int EOSLIB_SERIALIZE(eosio::metadata_code_joined, int block_num(int)(int)(int)(int)(int)(int)(int)(int)(int)(int)(int));
};
```


account_metadata and code joined



---



#### Struct <code>eosio::contract_row</code></strong>


```
struct contract_row
{
    uint32_t block_num = {};

    bool present = {};

    int code = {};

    int scope = {};

    int table = {};

    uint64_t primary_key = {};

    int payer = {};

    shared_memory<datastream<const char *>> value = {};
};
```


A row in a contract’s table



---



#### Struct <code>eosio::contract_secondary_index_with_row</code></strong>


```
template <typename T>
struct contract_secondary_index_with_row
{
    uint32_t block_num = {};

    bool present = {};

    int code = {};

    int scope = {};

    int table = {};

    uint64_t primary_key = {};

    int payer = {};

    T secondary_key = {};

    uint32_t row_block_num = {};

    bool row_present = {};

    int row_payer = {};

    shared_memory<datastream<const char *>> row_value = {};
};
```


A secondary index entry in a contract’s table. Also includes fields from `contract_row`.



---



#### Struct <code>eosio::query_block_info_range_index</code></strong>


```
struct query_block_info_range_index
{
    int query_name = "block.info"_n;

    uint32_t first = {};

    uint32_t last = {};

    uint32_t max_results = {};
};
```


Pass this to `query_database` to get `block_info` for a range of block indexes.

The query results are sorted by `block_num`. Every record has a different block_num.


##### Member variables



*   `query_name` - Identifies query type. Do not modify this field.
*   `first` - Query records with `block_num` in the range [`first`, `last`].
*   `last` - Query records with `block_num` in the range [`first`, `last`].
*   `max_results` - Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.



---



#### Struct <code>eosio::query_action_trace_range_name_receiver_account_block_trans_action</code></strong>


```
struct query_action_trace_range_name_receiver_account_block_trans_action
{
    struct key;

    int query_name = "at.e.nra"_n;

    uint32_t snapshot_block = {};

    eosio::query_action_trace_range_name_receiver_account_block_trans_action::key first = {};

    eosio::query_action_trace_range_name_receiver_account_block_trans_action::key last = {};

    uint32_t max_results = {};
};
```


Pass this to `query_database` to get `action_trace` for a range of keys.

The query results are sorted by `key`. Every record has a different key.


```
struct key {
    eosio::name     name             = {};
    eosio::name     receiver         = {};
    eosio::name     account          = {};
    uint32_t        block_num        = {};
    checksum256     transaction_id   = {};
    uint32_t        action_ordinal   = {};

    // Construct the key from `data`
    static key from_data(const action_trace& data);
};
```



##### Member variables



*   `query_name` - Identifies query type. Do not modify this field.
*   `snapshot_block` - Look at this point of time in history
*   `first` - Query records with keys in the range [`first`, `last`].
*   `last` - Query records with keys in the range [`first`, `last`].
*   `max_results` - Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.



---



#### Struct <code>eosio::query_action_trace_receipt_receiver</code></strong>


```
struct query_action_trace_receipt_receiver
{
    struct key;

    int query_name = "receipt.rcvr"_n;

    uint32_t snapshot_block = {};

    eosio::query_action_trace_receipt_receiver::key first = {};

    eosio::query_action_trace_receipt_receiver::key last = {};

    uint32_t max_results = {};
};
```


Pass this to `query_database` to get `action_trace` for a range of `receipt_receiver` names.

The query results are sorted by `key`. Every record has a unique key.


```
struct key {
    eosio::name     receipt_receiver = {};
    uint32_t        block_num        = {};
    checksum256     transaction_id   = {};
    uint32_t        action_ordinal   = {};

    // Construct the key from `data`
    static key from_data(const action_trace& data);
};
```



##### Member variables



*   `query_name` - Identifies query type. Do not modify this field.
*   `snapshot_block` - Look at this point of time in history
*   `first` - Query records with keys in the range [`first`, `last`].
*   `last` - Query records with keys in the range [`first`, `last`].
*   `max_results` - Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.



---



#### Struct <code>eosio::query_transaction_receipt</code></strong>


```
struct query_transaction_receipt
{
    struct key;

    int query_name = "transaction"_n;

    uint32_t snapshot_block = {};

    eosio::query_transaction_receipt::key first = {};

    eosio::query_transaction_receipt::key last = {};

    uint32_t max_results = {};
};
```


Pass this to `query_database` to get a transaction receipt for a transaction id.

The query results are sorted by `key`. Every record has a unique key.


```
struct key {
    checksum256 transaction_id = {};
    uint32_t block_num = {};
    uint32_t action_ordinal = {};

    // Construct the key from `data`
    static key from_data(const action_trace& data);
};
```



##### Member variables



*   `query_name` - Identifies query type. Do not modify this field.
*   `snapshot_block` - Look at this point of time in history
*   `first` - Query records with keys in the range [`first`, `last`].
*   `last` - Query records with keys in the range [`first`, `last`].
*   `max_results` - Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.



---



#### Struct <code>eosio::query_account_range_name</code></strong>


```
struct query_account_range_name
{
    int query_name = "account"_n;

    uint32_t snapshot_block = {};

    int first = {};

    int last = {};

    uint32_t max_results = {};
};
```


Pass this to `query_database` to get `account` for a range of names.

The query results are sorted by `name`. Every record has a different name.


##### Member variables



*   `query_name` - Identifies query type. Do not modify this field.
*   `snapshot_block` - Look at this point of time in history
*   `first` - Query records with `name` in the range [`first`, `last`].
*   `last` - Query records with `name` in the range [`first`, `last`].
*   `max_results` - Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.



---



#### Struct <code>eosio::query_acctmeta_range_name</code></strong>


```
struct query_acctmeta_range_name
{
    int query_name = "acctmeta.jn"_n;

    uint32_t snapshot_block = {};

    int first = {};

    int last = {};

    uint32_t max_results = {};
};
```


Pass this to `query_database` to get `account_metadata_joined` for a range of names.

The query results are sorted by `name`. Every record has a different name.


##### Member variables



*   `query_name` - Identifies query type. Do not modify this field.
*   `snapshot_block` - Look at this point of time in history
*   `first` - Query records with `name` in the range [`first`, `last`].
*   `last` - Query records with `name` in the range [`first`, `last`].
*   `max_results` - Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.



---



#### Struct <code>eosio::query_code_range_name</code></strong>


```
struct query_code_range_name
{
    int query_name = "meta.jn.code"_n;

    uint32_t snapshot_block = {};

    int first = {};

    int last = {};

    uint32_t max_results = {};
};
```


Pass this to `query_database` to get `metadata_code_joined` for a range of names.

The query results are sorted by `name`. Every record has a different name.


##### Member variables



*   `query_name` - Identifies query type. Do not modify this field.
*   `snapshot_block` - Look at this point of time in history
*   `first` - Query records with `name` in the range [`first`, `last`].
*   `last` - Query records with `name` in the range [`first`, `last`].
*   `max_results` - Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.



---



#### Struct <code>eosio::query_contract_row_range_code_table_pk_scope</code></strong>


```
struct query_contract_row_range_code_table_pk_scope
{
    struct key;

    int query_name = "cr.ctps"_n;

    uint32_t snapshot_block = {};

    eosio::query_contract_row_range_code_table_pk_scope::key first = {};

    eosio::query_contract_row_range_code_table_pk_scope::key last = {};

    uint32_t max_results = {};
};
```


Pass this to `query_database` to get `contract_row` for a range of keys.

The query results are sorted by `key`. Every record has a different key.


```
struct key {
    name     code        = {};
    name     table       = {};
    uint64_t primary_key = {};
    name     scope       = {};

    // Construct the key from `data`
    static key from_data(const contract_row& data);
};
```



##### Member variables



*   `query_name` - Identifies query type. Do not modify this field.
*   `snapshot_block` - Look at this point of time in history
*   `first` - Query records with keys in the range [`first`, `last`].
*   `last` - Query records with keys in the range [`first`, `last`].
*   `max_results` - Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.



---



#### Struct <code>eosio::query_contract_row_range_code_table_scope_pk</code></strong>


```
struct query_contract_row_range_code_table_scope_pk
{
    struct key;

    int query_name = "cr.ctsp"_n;

    uint32_t snapshot_block = {};

    eosio::query_contract_row_range_code_table_scope_pk::key first = {};

    eosio::query_contract_row_range_code_table_scope_pk::key last = {};

    uint32_t max_results = {};
};
```


Pass this to `query_database` to get `contract_row` for a range of keys.

The query results are sorted by `key`. Every record has a different key.


```
struct key {
    name     code        = {};
    name     table       = {};
    name     scope       = {};
    uint64_t primary_key = {};

    // Construct the key from `data`
    static key from_data(const contract_row& data);
};
```



##### Member variables



*   `query_name` - Identifies query type. Do not modify this field.
*   `snapshot_block` - Look at this point of time in history
*   `first` - Query records with keys in the range [`first`, `last`].
*   `last` - Query records with keys in the range [`first`, `last`].
*   `max_results` - Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.



---



#### Struct <code>eosio::query_contract_row_range_scope_table_pk_code</code></strong>


```
struct query_contract_row_range_scope_table_pk_code
{
    struct key;

    int query_name = "cr.stpc"_n;

    uint32_t snapshot_block = {};

    eosio::query_contract_row_range_scope_table_pk_code::key first = {};

    eosio::query_contract_row_range_scope_table_pk_code::key last = {};

    uint32_t max_results = {};
};
```


Pass this to `query_database` to get `contract_row` for a range of keys.

The query results are sorted by `key`. Every record has a different key.


```
struct key {
    name     scope       = {};
    name     table       = {};
    uint64_t primary_key = {};
    name     code        = {};

    // Construct the key from `data`
    static key from_data(const contract_row& data);
};
```



##### Member variables



*   `query_name` - Identifies query type. Do not modify this field.
*   `snapshot_block` - Look at this point of time in history
*   `first` - Query records with keys in the range [`first`, `last`].
*   `last` - Query records with keys in the range [`first`, `last`].
*   `max_results` - Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.



---



#### Struct <code>eosio::query_contract_index64_range_code_table_scope_sk_pk</code></strong>


```
struct query_contract_index64_range_code_table_scope_sk_pk
{
    struct key;

    int query_name = "ci1.cts2p"_n;

    uint32_t snapshot_block = {};

    eosio::query_contract_index64_range_code_table_scope_sk_pk::key first = {};

    eosio::query_contract_index64_range_code_table_scope_sk_pk::key last = {};

    uint32_t max_results = {};
};
```


Pass this to `query_database` to get `contract_secondary_index_with_row&lt;uint64_t>` for a range of keys.

The query results are sorted by `key`. Every record has a different key.


```
struct key {
    name     code          = {};
    name     table         = {};
    name     scope         = {};
    uint64_t secondary_key = {};
    uint64_t primary_key   = {};

    // Construct the key from `data`
    static key from_data(const contract_secondary_index_with_row<uint64_t>& data);
};
```



##### Member variables



*   `query_name` - Identifies query type. Do not modify this field.
*   `snapshot_block` - Look at this point of time in history
*   `first` - Query records with keys in the range [`first`, `last`].
*   `last` - Query records with keys in the range [`first`, `last`].
*   `max_results` - Maximum results to return. The wasm-ql server may cap the number of results to a smaller number.



---



#### Struct <code>eosio::database_status</code></strong>


```
struct database_status
{
    uint32_t head = {};

    eosio::checksum256 head_id = {};

    uint32_t irreversible = {};

    eosio::checksum256 irreversible_id = {};

    uint32_t first = {};
};
```


Status of the database. Returned by `get_database_status`.



---



#### Function <code>eosio::get_database_status</code></strong>


```
eosio::database_status get_database_status();
```


Get the current database status



---



#### Function <code>eosio::query_database</code></strong>


```
template <typename T>
int query_database();
```


Query the database. `request` must be one of the `query_*` structs. Returns result in serialized form.

The serialized form is the same as `vector&lt;vector&lt;char>>`’s serialized form. Each inner vector contains the serialized form of a record. The record type varies with query.

Use `for_each_query_result` or `for_each_contract_row` to iterate through the result.



---



#### Function <code>eosio::for_each_query_result</code></strong>


```
template <typename T, typename F>
bool for_each_query_result(int const& bytes, F f);
```


Unpack each record of a query result and call `f(record)`. `T` is the record type.



---



#### Function <code>eosio::for_each_contract_row</code></strong>


```
template <typename T, typename F>
bool for_each_contract_row(int const& bytes, F f);
```


Use with `query_contract_row_*`. Unpack each row of a query result and call `f(row, data)`. `row` is an instance of `contract_row`. `data` is the unpacked contract-specific data. `T` identifies the type of `data`.
