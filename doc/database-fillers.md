# Database Fillers

The database fillers connect to the nodeos state-history plugin and populate databases.

## PostgreSQL vs. RocksDB

* PostgreSQL
  * Supports full history
  * Partial history can fall behind on large chains; PostgreSQL sometimes struggles to delete large numbers of rows
  * Scaling: supports wasm-ql running on multiple machines connecting to a single database
* RocksDB
  * Supports full and partial history
  * Simpler setup; RocksDB is an in-process database
  * Saves disk space compared to PostgreSQL
  * Faster filling than PostgreSQL
  * Scaling: each machine hosting wasm-ql servers has a separate database

## Running fillers

When running `fill-pg` for the first time, use the `--fpg-create` option to create the schema and tables. To wipe the schema and start over, run with `--fpg-drop --fpg-create`. 

`fill-rocksdb` and `combo-rocksdb` automatically create a database if it doesn't exist; it doesn't have `drop` or `create` options.

After starting, a filler will populate the database. It will track real-time updates from nodeos after it catches up.

Use SIGINT or SIGTERM to stop.

## Option matrix

| RocksDB fill          | PostgreSQL fill           | Default               | Description |
|---------------------  |-------------------------- |--------------------   |-------------|
| --fill-connect-to     | --fill-connect-to         | 127.0.0.1:8080        | state-history-plugin endpoint to connect to |
|                       | --pg-schema               | chain                 | schema to use |
| --rdb-database        |                           |                       | database path |
| --rdb-threads         |                           |                       | Increase number of background RocksDB threads. Recommend 8 for full history on large chains |
| --rdb-max-files       |                           |                       | Limit max number of open files (default unlimited). This should be smaller than 'ulimit -n #'. # should be a very large number for full-history nodes. |
| --query-config        |                           |                       | query configuration file |
|                       | --fpg-drop                |                       | drop (delete) schema and tables |
|                       | --fpg-create              |                       | create schema and tables |
| --fill-trim           | --fill-trim               |                       | trim history before irreversible |
| --fill-skip-to        | --fill-skip-to            |                       | skip blocks before arg |
| --fill-stop           | --fill-stop               |                       | stop filling at block arg |
| --fill-trx            | --fill-trx                |                       | filter transactions |

## Transaction filters

`--fill-trx` creates a set of transaction filtering rules. It has the following syntax:

```
--fill-trx include:status:receiver:act_account:act_name
```

It ignores whitespace within the pattern.

| Field         | May be empty? | Description |
| ------------- | ------------- | ----------- |
| include       | No            | "`+`" to pass a matching action, or "`-`" to not pass |
| status        | Yes           | Transaction status. May be one of: `executed`, `soft_fail`, `hard_fail`, `delayed`, `expired` |
| receiver      | Yes           | The account which originally received the action, or the account which received a copy (`require_recipient`). |
| act_account   | Yes           | The account which received the original. This is called `code` or `first_receiver` in the CDT. |
| act_name      | Yes           | The name of the action |

`--fill-trx` may be specified multiple times. This creates a list of rules. The filter checks an action against each
rule in order. As soon as it finds a rule which matches the action it stops. The action passes if `include` is `+`. 
The action doesn't pass if `include` is `-`. If no rules match, then the action doesn't pass.

The filler writes a transaction to the database if any of the transaction's actions pass the filter. When this happens, it writes all
actions in the transaction, including ones that didn't pass.

### Transaction filter examples

* Include all transactions. Includes deferred transactions which haven't executed
  yet or have failed. This is the default if no `--fill-trx` is provided:

```
--fill-trx "+:        :            :            :"
```

* Include all executed transactions. Excludes deferred transactions which haven't executed
  yet or have failed:

```
--fill-trx "+:executed:            :            :"
```

* Include all executed transactions, but exclude some spam:

```
--fill-trx "-:        :blocktwitter:blocktwitter:"
--fill-trx "+:executed:            :            :"
```

* Include all executed transfers. Includes all token contracts:

```
--fill-trx "+:executed:            :            :transfer"
```

* Include all executed transfers. Includes only `eosio.token`:

```
--fill-trx "+:executed:            :eosio.token :transfer"
```

* Include all executed transfers which notify specific accounts. Includes all token contracts:

```
--fill-trx "+:executed:myaccount1  :            :transfer"
--fill-trx "+:executed:myaccount2  :            :transfer"
```

* Include all executed transfers which notify specific accounts. Only includes `eosio.token`:

```
--fill-trx "+:executed:myaccount1  :eosio.token :transfer"
--fill-trx "+:executed:myaccount2  :eosio.token :transfer"
```

## PostgreSQL configuration

fill-pg relies on PostgreSQL environment variables to establish connections; see the PostgreSQL manual.

A quick-and-dirty way to connect to PostgreSQL server running on another machine is to set these:
* PGUSER
* PGPASSWORD
* PGDATABASE
* PGHOST

Use the `psql` utility to verify your connection.
