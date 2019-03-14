# Database Fillers

The database fillers connect to the nodeos state-history plugin and populate databases. 

## PostgreSQL vs. LMDB

* PostgreSQL
  * Supports full history
  * Partial history can fall behind on large chains; PostgreSQL sometimes struggles to delete large numbers of rows
  * Scaling: supports wasm-ql running on multiple machines connecting to a single database
* LMDB
  * Supports partial history
  * Full history not recommended for large chains
  * Simpler setup; LMDB is an in-process database
  * Scaling: each machine has a separate database

## Running fillers

When running `fill-pg` for the first time, use the `--fpg-create` option to create the schema and tables. To wipe the schema and start over, run with `--fpg-drop --fpg-create`. 

`fill-lmdb` automatically creates a database if it doesn't exist; it doesn't have `drop` or `create` options.

After starting, a filler will populate the database. It will track real-time updates from nodeos after it catches up.

Use SIGINT or SIGTERM to stop.

## Option matrix

| LMDB fill             | PostgreSQL fill           | Default               | Description |
|---------------------  |-------------------------- |--------------------   |-------------|
| --fill-connect-to     | --fill-connect-to         | localhost:8080        | state-history-plugin endpoint to connect to |
|                       | --pg-schema               | chain                 | schema to use |
| --lmdb-database       |                           |                       | database path |
| --lmdb-set-db-size-gb |                           |                       | set maximum database size |
| --query-config        |                           |                       | query configuration file |
|                       | --fpg-drop                |                       | drop (delete) schema and tables |
|                       | --fpg-create              |                       | create schema and tables |
| --fill-trim           | --fill-trim               |                       | trim history before irreversible |
| --fill-skip-to        | --fill-skip-to            |                       | skip blocks before arg |
| --fill-stop           | --fill-stop               |                       | stop filling at block arg |

## PostgreSQL configuration

fill-postgresql relies on PostgreSQL environment variables to establish connections; see the PostgreSQL manual.

A quick-and-dirty way to connect to PostgreSQL server running on another machine is to set these:
* PGUSER
* PGPASSWORD
* PGDATABASE
* PGHOST

Use the `psql` utility to verify your connection.
