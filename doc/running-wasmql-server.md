# Running a wasm-ql Server

A wasm-ql system needs:
* nodeos running the state-history plugin, with either full or recent history
* A database: PostgreSQL or RocksDB
* A database filler
* 1 or more wasm-ql server processes

## Minimal RocksDB-based system

* `combo-rocksdb` fills an RocksDB database and has a single-process, single-thread wasm-ql server.
* Suitable for single-developer testing

## Multiple-process RocksDB-based system

* `fill-rocksdb` fills an RocksDB database. Run 1 instance of this on a machine.
* `wasm-ql-rocksdb` uses the database to answer queries. It serves requests from the main thread; to scale it, run multiple instances of this on the same machine as `fill-rocksdb`.

## Multiple-process PostgreSQL-based system

* `fill-pg` fills a PostgreSQL database. Run 1 instance of this.
* `wasm-ql-pg` uses the database to answer queries. It serves requests from the main thread; to scale it, run multiple instances of this, either on 1 machine, or spread across several.

## Connecting to a database

wasm-ql servers use the same connection methods and options as the [database fillers](database-fillers.md).

* PostgreSQL: `fill-pg` sets up a bare database without indexes and query functions. After `fill-pg` is caught up to the chain, stop it then run `init.sql` in this repository's source directory. e.g. `psql -f path/to/init.sql`.
* RocksDB: `fill-rocksdb` and `combo-rocksdb` create a full set of indexes.

## Testing wasm-ql

This uses the legacy API (/v1/...) to verify wasm-ql is operational. It queries active name bids on the system contract.

```
cd build
curl localhost:8880/v1/chain/get_table_rows -d '{"code":"eosio", "scope":"eosio", "table":"namebids", "show_payer":true, "json":true, "key_type": "name", "index_position": "2", "limit":100}' | json_pp
node ../src/test-client.js
```

## Option matrix

Options:

| RocksDB wasm-ql       | PostgreSQL wasm-ql        | Default               | Description |
|---------------------  |-------------------------- |--------------------   |-------------|
| --wql-listen          | --wql-listen              | localhost:8880        | Endpoint to listen for incoming queries |
| --wql-wasm-dir        | --wql-wasm-dir            | .                     | Directory to fetch WASMs from |
|                       | --pg-schema               | chain                 | Schema to use |
| --rdb-database        |                           |                       | Database path |
| --rdb-secondary       |                           |                       | Path for secondary database instance. This database automatically tails the primary database. |
| --query-config        | --query-config            |                       | Query configuration file |
