# History Tools

| App             | Fills LMDB | wasm-ql with LMDB          | Fills Postgresql | wasm-ql with Postgresql |
| --------------- | ---------- | -------------------------- | ---------------- | ---------------- |
| `fill-lmdb`     | Yes        |                            |                  |                  |        
| `wasm-ql-lmdb`  |            | Yes                        |                  |                  |            
| `combo-lmdb`    | Yes        | Yes                        |                  |                  |            
| `fill-pg`       |            |                            | Yes              |                  |        
| `wasm-ql-pg`    |            |                            |                  | Yes              |            
| `history-tools` | Yes*       | Yes*                       | Yes*             | Yes*             |            

Note: by default, `history-tools` does nothing; use the `--plugin` option to select plugins.

# Postgresql vs. lmdb

* postgresql
  * Supports full history
  * Partial history can fall behind; postgresql sometimes struggles to delete large numbers of rows
  * Scaling: supports wasm-ql running on multiple machines connecting to a single database
* lmdb
  * Supports partial history
  * Full history untested
  * Simpler setup; lmdb is an in-process database
  * Scaling: each machine has a separate database

# Build

* [Ubuntu 1810](doc/build-ubuntu-1810.md)
* [OSX](doc/build-osx.md)

# Wasm-ql system configurations

A wasm-ql system needs:
* nodeos running the state-history plugin, with either full or recent history
* A database: postgresql or lmdb
* A database filler
* 1 or more wasm-ql server processes

## Minimal lmdb-based system

* `combo-lmdb` fills an lmdb database and has a single-process, single-thread wasm-ql server.
* Suitable for single-developer testing

## Multiple-process lmdb-based system

* `fill-lmdb` fills an lmdb database. Run 1 instance of this on a machine.
* `wasm-ql-lmdb` uses the database to answer queries. It serves requests from the main thread; to scale it, run multiple instances of this on the same machine as `fill-lmdb`

## Multiple-process postgresql-based system

* `fill-pg` fills a postgresql database. Run 1 instance of this.
* `wasm-ql-pg` uses the database to answer queries. It serves requests from the main thread; to scale it, run multiple instances of this, either on 1 machine, or spread across several.

# Option matrix

Options:

| lmdb fill             | postgresql fill           | lmdb wasm-ql          | postgresql wasm-ql        | Default               | Description |
|---------------------  |-------------------------- |---------------------  |-------------------------- |--------------------   |-------------|
| --fill-connect-to     | --fill-connect-to         |                       |                           | localhost:8080        | state-history-plugin endpoint to connect to |
|                       |                           | --wql-listen          | --wql-listen              | localhost:8880        | endpoint to listen for incoming queries |
|                       | --pg-schema               |                       | --pg-schema               | chain                 | schema to use |
| --lmdb-database       |                           | --lmdb-database       |                           |                       | database path |
| --lmdb-set-db-size-gb |                           |                       |                           |                       | set maximum database size |
| --query-config        |                           | --query-config        | --query-config            |                       | query configuration file |
|                       | --fpg-drop                |                       |                           |                       | drop (delete) schema and tables |
|                       | --fpg-create              |                       |                           |                       | create schema and tables |
| --fill-trim           | --fill-trim               |                       |                           |                       | trim history before irreversible |
| --fill-skip-to        | --fill-skip-to            |                       |                           |                       | skip blocks before arg |
| --fill-stop           | --fill-stop               |                       |                           |                       | stop filling at block arg |

# Running fillers

When running `fill-pg` for the first time, use the `--fpg-create` option to create the schema and tables. To wipe the schema and start over, run with `--fpg-drop --fpg-create`.

After starting, a filler will populate the database. It will track real-time updates from nodeos after it catches up.

Use SIGINT or SIGTERM to stop.

## Nodeos configuration

| Option                                    | When to use |
|-------------------------------------------|-------------|
| `--plugin eosio::state_history_plugin`    | always |
| `--state-history-endpoint`                | optional; defaults to 127.0.0.1:8080 |
| `--trace-history`                         | needed for wasm-ql; enable to collect transaction and action traces |
| `--chain-state-history`                   | needed for wasm-ql; enable to collect state (tables) |

Caution: either use a firewall to block access to the state-history endpoint, or leave it as `127.0.0.1:8080` to disable remote access.

## Postgresql configuration

fill-postgresql relies on postgresql environment variables to establish connections; see the postgresql manual.

A quick-and-dirty way to connect to postgresql server running on another machine is to set these:
* PGUSER
* PGPASSWORD
* PGDATABASE
* PGHOST

Use the `psql` utility to verify your connection.

## Fast start without history on existing chains

This option creates a database which tracks the chain state, but lacks most historical information.

* Get the following:
  * A portable snapshot (`data/snapshots/snapshot-xxxxxxx.bin`)
  * Optional: a block log which includes the block the snapshot was taken at
* Make sure `data/state` does not exist
* Start nodeos with the `--snapshot` option, and the options listed in "Nodeos configuration" above
* Look for `Placing initial state in block n` in the log
* Start a filler with `--fpg-create` (if postgresql), `--*-skip-to n`, and `--*-trim`. Replace `n` with the value above.
* Do not stop nodeos until it has received at least 1 block from the network, or it won't be able to restart.

If nodeos fails to receive blocks from the network, then try the above using `net_api_plugin`. Use `cleos net disconnect` and `cleos net connect` to reconnect nodes which timed out. Caution when using net_api_plugin: either use a firewall to block access to `http-server-address`, or change it to `localhost:8888` to disable remote access.

Whenever you run a filler after this point, use the `--*-trim` option. Only use `--fpg-create` and `--*-skip-to` the first time.

## nodeos: creating a portable snapshot with full state-history

* Enable the `producer_api_plugin` on a node with full state-history. Caution when using producer_api_plugin: either use a firewall to block access to `http-server-address`, or change it to `localhost:8888` to disable remote access.
* Create a portable snapshot: `curl http://127.0.0.1:8888/v1/producer/create_snapshot | json_pp`
* Wait for nodeos to process several blocks after the snapshot completed. The goal is for the state-history files to contain at least 1 more block than the portable snapshot has, and for the block log to contain the block after it has become irreversible.
* Note: if the block included in the portable snapshot is forked out, then the snapshot will be invalid. Repeat this process if this happens.
* Stop nodeos
* Make backups of:
  * The newly-created portable snapshot (`data/snapshots/snapshot-xxxxxxx.bin`)
  * The contents of `data/state-history`:
    * `chain_state_history.log`
    * `trace_history.log`
    * `chain_state_history.index`: optional. Restoring will take longer without this file.
    * `trace_history.index`: optional. Restoring will take longer without this file.
  * Optional: the contents of `data/blocks`, but excluding `data/blocks/reversible`.

## nodeos: restoring a portable snapshot with full state-history

* Get the following:
  * A portable snapshot (`data/snapshots/snapshot-xxxxxxx.bin`)
  * The contents of `data/state-history`
  * Optional: a block log which includes the block the snapshot was taken at. Do not include `data/blocks/reversible`.
* Make sure `data/state` does not exist
* Start nodeos with the `--snapshot` option, and the options listed in "Nodeos configuration" above
* Do not stop nodeos until it has received at least 1 block from the network, or it won't be able to restart.

If nodeos fails to receive blocks from the network, then try the above using `net_api_plugin`. Use `cleos net disconnect` and `cleos net connect` to reconnect nodes which timed out. Caution when using net_api_plugin: either use a firewall to block access to `http-server-address`, or change it to `localhost:8888` to disable remote access.

# Testing wasm-ql
```
cd build
curl localhost:8880/v1/chain/get_table_rows -d '{"code":"eosio", "scope":"eosio", "table":"namebids", "show_payer":true, "json":true, "key_type": "name", "index_position": "2", "limit":100}' | json_pp
node ../src/test-client.js
```
