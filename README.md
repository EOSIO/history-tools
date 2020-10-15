[![Gitpod ready-to-code](https://img.shields.io/badge/Gitpod-ready--to--code-blue?logo=gitpod)](https://gitpod.io/#https://github.com/EOSIO/history-tools)

# EOSIO History Tools ![EOSIO Alpha](https://img.shields.io/badge/EOSIO-Alpha-blue.svg)

[![Software License](https://img.shields.io/badge/license-MIT-lightgrey.svg)](./LICENSE)

The history tools repo has these components:

* Database fillers connect to the nodeos state-history plugin and populate databases
* wasm-ql servers answer incoming queries by running server WASMs, which have read-only access to the databases
* The wasm-ql library, when combined with the CDT library, provides utilities that server WASMs and client WASMs need
* A set of example server WASMs and client WASMs

| App               | Fills RocksDB | wasm-ql with RocksDB       | Fills PostgreSQL | wasm-ql with PostgreSQL |
| ----------------- | ------------- | -------------------------- | ---------------- | ---------------- |
| `fill-rocksdb`    | Yes           |                            |                  |                  |        
| `wasm-ql-rocksdb` |               | Yes                        |                  |                  |            
| `combo-rocksdb`   | Yes           | Yes                        |                  |                  |            
| `fill-pg`         |               |                            | Yes              |                  |        
| `wasm-ql-pg`      |               |                            |                  | Yes              |            
| `history-tools`   | Yes*          | Yes*                       | Yes*             | Yes*             |            

Note: by default, `history-tools` does nothing; use the `--plugin` option to select plugins.

See the [documentation site](https://eosio.github.io/history-tools/)

# Alpha Release

This is an alpha release of the EOSIO History Tools. It includes database fillers
(`fill-pg`, `fill-rocksdb`) which pull data from nodeos's State History Plugin, and a new
query engine (`wasm-ql-pg`, `wasm-ql-rocksdb`) which supports queries defined by wasm, along
with an emulation of the legacy `/v1/` RPC API.

This alpha release is designed to solicit community feedback. There are several potential
directions this toolset may take; we'd like feedback on which direction(s) may be most
useful. Please create issues about changes you'd like to see going forward.

Since this is an alpha release, it will likely have incompatible changes in the
future. Some of these may be driven by community feedback.

This release supports nodeos 1.8.x. It does not support 1.7.x or the 1.8 RC versions. This release
includes the following:

## Alpha 0.3.0

This release adds temporary workarounds to `fill-pg` to support Nodeos 2.0. It also disables the remaining tools. If you would
like to test rocksdb support or wasm-ql support, stick with Nodeos 1.8 and the Alpha 0.2.0 release of History Tools.

* Temporary `fill-pg` fixes
  * Removed the `global_property` table
  * Removed `new_producers` from the `block_info` table
* Temporarily disabled building everything except `fill-pg`

## Alpha 0.2.0

* There are now 2 self-contained demonstrations in public Docker images. See [container-demos](doc/container-demos.md) for details.
  * Talk: this demonstrates using wasm-ql to provide messages from on-chain conversations to clients in threaded order.
  * Partial history: this demonstrates some of wasm-ql's chain and token queries on data drawn from one of the public EOSIO networks.
* Added RocksDB and removed LMDB. This has the following advantages:
  * Filling outperforms both PostgreSQL and LMDB by considerable margins, both for partial history
    and for full history on large well-known chains.
  * Database size for full history is much smaller than PostgreSQL.
* Database fillers have a new option `--fill-trx` to filter transaction traces.
* Database fillers no longer need `--fill-skip-to` when starting from partial history.
* Database fillers now automatically reconnect to the State History Plugin.
* wasm-ql now uses a thread pool to handle queries. `--wql-threads` controls the thread pool size.
* wasm-ql now uses eos-vm instead of SpiderMonkey. This simplifies the build process.
* wasm-ql can now serve static files. Enabled by the new `--wql-static-dir` option.
* SHiP connection handling moved to `state_history_connection.hpp`. This file may aid users needing
  to write custom solutions which connect to the State History Plugin.

## fill-pg

`fill-pg` fills postgresql with data from nodeos's State History Plugin. It provides nearly all
data that applications which monitor the chain need. It provides the following:

* Header information from each block
* Transaction and action traces, including inline actions and deferred transactions
* Contract table history, at the block level
* Tables which track the history of chain state, including
  * Accounts, including permissions and linkauths
  * Account resource limits and usage
  * Contract code
  * Contract ABIs
  * Consensus parameters
  * Activated consensus upgrades

`fill-pg` keeps action data and contract table data in its original binary form. Future versions
may optionally support converting this to JSON.

To conserve space, `fill-pg` doesn't store blocks in postgresql. The majority of apps
don't need the blocks since:

* Blocks don't include inline actions and only include some deferred transactions. Most
  applications need to handle these, so should examine the traces instead. e.g. many transfers
  live in the inline actions and deferred transactions that blocks exclude.
* Most apps don't verify block signatures. If they do, then they should connect directly to
  nodeos's State History Plugin to get the necessary data. Note that contrary to
  popular belief, the data returned by the `/v1/get_block` RPC API is insufficient for
  signature verification since it uses a lossy JSON conversion.
* Most apps which currently use the `/v1/get_block` RPC API (e.g. `eosjs`) only need a tiny
  subset of the data within block; `fill-pg` stores this data. There are apps which use
  `/v1/get_block` incorrectly since their authors didn't realize the blocks miss
  critical data that their applications need.

`fill-pg` supports both full history and partial history (`trim` option). This allows users
to make their own tradeoffs. They can choose between supporting queries covering the entire
history of the chain, or save space by only covering recent history.

## wasm-ql-pg

EOSIO contracts store their data in a format which is convenient for them, but hard
on general-purpose query engines. e.g. the `/v1/get_table_rows` RPC API struggles to provide 
all the necessary query options that applications need. `wasm-ql-pg` allows contract authors
and application authors to design their own queries using the same 
[toolset](https://github.com/EOSIO/eosio.cdt) that they use to design contracts. This
gives them full access to current contract state, a history of contract state, and the
history of actions for that contract. `fill-pg` preserves this data in its original
format to support `wasm-ql-pg`.

wasm-ql supports two kinds of queries:
* Binary queries are the most flexible. A client-side wasm packs the query into a binary
  format, a server-side wasm running in wasm-ql executes the query then produces a result
  in binary format, then the client-side wasm converts the binary to JSON. The toolset
  helps authors create both wasms.
* An emulation of the `/v1/` RPC API.

We're considering dropping client-side wasms and switching the format of the first type
of query to JSON RPC, Graph QL, or another format. We're seeking feedback on this switch.

## combo-rocksdb, fill-rocksdb, wasm-ql-rocksdb

These function identically to `fill-pg` and `wasm-ql-pg`, but store data using RocksDB
instead of postgresql. Since RocksDB is an embedded database instead of a database server,
this option may be simpler to administer. RocksDB also saves space and fills quicker.

* `combo-rocksdb`: Fills the database and answers queries. Use this for queries against a live database.
* `fill-rocksdb`: Use this when filling a database for the first time. It fills faster
   than `combo-rocksdb` but can't answer queries. Switch to `combo-rocksdb` after the database
   catches up with the chain.
* `wasm-ql-rocksdb`: Rarely used. Queries a database that isn't being filled.

## Contributing

[Contributing Guide](./CONTRIBUTING.md)

[Code of Conduct](./CONTRIBUTING.md#conduct)

## License

[MIT](./LICENSE)

## Important

See [LICENSE](LICENSE) for copyright and license terms.

All repositories and other materials are provided subject to the terms of this [IMPORTANT](important.md) notice and you must familiarize yourself with its terms.  The notice contains important information, limitations and restrictions relating to our software, publications, trademarks, third-party resources, and forward-looking statements.  By accessing any of our repositories and other materials, you accept and agree to the terms of the notice.
