---
content_title: Running a wasm-ql Server
---

A wasm-ql system needs:
* nodeos running the state-history plugin, with either full or recent history
* A database: PostgreSQL or RocksDB
* A database filler
* 1 or more wasm-ql server processes

## RocksDB-based system

`combo-rocksdb` fills a RocksDB database and processes wasm-ql requests on multiple threads.

## PostgreSQL-based system

* `fill-pg` fills a PostgreSQL database.
* `wasm-ql-pg` uses the database to answer queries. It processes wasm-ql requests on multiple threads.

## Connecting to a database

wasm-ql servers use the same connection methods and options as the [database fillers](database-fillers.md).

* PostgreSQL: `fill-pg` sets up a bare database without indexes and query functions. After `fill-pg` is caught up to the chain, stop it then run `init.sql` in this repository's source directory. e.g. `psql -f path/to/init.sql`.
* RocksDB: `fill-rocksdb` and `combo-rocksdb` automatically create a full set of indexes.

## Testing wasm-ql

```
cd build
curl localhost:8880/v1/chain/get_table_rows -d '{"code":"eosio", "scope":"eosio", "table":"namebids", "show_payer":true, "json":true, "key_type": "name", "index_position": "2", "limit":100}' | json_pp
node ../src/test-client.js
```

## Option matrix

Options:

| RocksDB wasm-ql       | PostgreSQL wasm-ql        | Default               | Description |
|---------------------  |-------------------------- |--------------------   |-------------|
| --wql-threads         | --wql-threads             | 8                     | Number of threads to process requests |
| --wql-listen          | --wql-listen              | 127.0.0.1:8880        | Endpoint to listen for incoming queries |
| --wql-allow-origin    | --wql-allow-origin        |                       | Access-Control-Allow-Origin header. Use "*" to allow any. |
| --wql-wasm-dir        | --wql-wasm-dir            | .                     | Directory to fetch WASMs from |
| --wql-static-dir      | --wql-static-dir          | (disabled)            | Directory to serve static files from |
| --wql-console         | --wql-console             | (disabled)            | Show console output |
|                       | --pg-schema               | chain                 | Schema to use |
| --rdb-database        |                           |                       | Database path |
| --rdb-threads         |                           |                       | Increase number of background RocksDB threads. Recommend 8 for full history on large chains |
| --rdb-max-files       |                           |                       | Limit max number of open files (default unlimited). This should be smaller than 'ulimit -n #'. # should be a very large number for full-history nodes. |
| --query-config        | --query-config            |                       | Query configuration file |
