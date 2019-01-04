## Running fill-postgresql

Notable configuration options:

| Option    | Default   | Description |
|-----------|-----------|-------------|
| --host    | localhost | state-history-plugin host to connect to |
| --port    | 8080      | state-history-plugin port to connect to |
| --schema  | chain     | Database schema to fill |
| --trim    |           | Trim history before irreversible |

When running it for the first time, use the `--create` option to create the schema and tables.

To wipe the schema and start over, run with `--drop --create`.

fill-postgresql will start filling the database. It will track real-time updates from nodeos after it catches up.

## Stopping

Use SIGINT or SIGTERM to stop.

## Build

Install the following:
* cmake
* A C++17 compiler
* Boost 1.58
* libpqxx
* libpq

Run the following.

```
git submodule update --init --recursive
mkdir build
cd build
cmake ..
make -j
```

## Build Example: Ubuntu 18.10 Server

Run the following on a fresh Ubuntu 18.10 image:

```
sudo apt update
sudo apt upgrade
sudo apt install build-essential cmake libboost-all-dev git libpq-dev libpqxx-dev
git clone git@github.com:EOSIO/fill-postgresql.git
cd fill-postgresql
git submodule update --init --recursive
mkdir build
cd build
cmake ..
make -j
```


## Nodeos configuration

| Option                                    | When to use |
|-------------------------------------------|-------------|
| `--plugin eosio::state_history_plugin`    | always |
| `--state-history-endpoint`                | optional; defaults to 0.0.0.0:8080 |
| `--trace-history`                         | optional; enable to collect transaction and action traces |
| `--chain-state-history`                   | optional; enable to collect state (tables) |

Caution: either use a firewall to block access to the state-history endpoint, or change it to `localhost:8080` to disable remote access.

## Postgresql configuration

fill-postgresql relies on postgresql environment variables to establish connections; see the postgresql manual.

A quick-and-dirty way to connect to postgresql server running on another machine is to set these:
* PGUSER
* PGPASSWORD
* PGDATABASE
* PGHOST

Use the `psql` utility to verify your connection.

## Fast start without history on existing chains

* Get the following:
  * A portable snapshot (`data/snapshots/snapshot-xxxxxxx.bin`)
  * Optional: a block log which includes the block the snapshot was taken at
* Make sure `data/state` does not exist
* Start nodeos with the `--snapshot` option, and the options listed in "Nodeos configuration" above
* Look for `Placing initial state in block n` in the log
* Start fill-postgresql with `--create`, `--skip-to n`, and `--trim`. Replace `n` with the value above.
* Do not stop nodeos until it has received at least 1 block from the network, or it won't be able to restart.

If nodeos fails to receive blocks from the network, then try the above using `net_api_plugin`. Use `cleos net disconnect` and `cleos net connect` to reconnect nodes which timed out. Caution when using net_api_plugin: either use a firewall to block access to `http-server-address`, or change it to `localhost:8888` to disable remote access.

Whenever you run fill-postgresql after this point, use the `--trim` option. Only use `--create` and `--skip-to` the first time.

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
