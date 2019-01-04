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

Caution: either use a firewall to block access to the state-history endpoint, or change it to `localhost:8080`

## Postgresql configuration

fill-postgresql relies on postgresql environment variables to establish connections; see the postgresql manual.

A quick-and-dirty way to connect to postgresql server running on another machine is to set these:
* PGUSER
* PGPASSWORD
* PGDATABASE
* PGHOST

Use the `psql` utility to verify your connection.

## Fast start without history on existing chains

* Get a recent portable snapshot file (`snapshot-xxxxxxx.bin`)
* Make sure the `data` folder does not exist
* Start nodeos with the `--snapshot` option, and the options listed in "Nodeos configuration" above
* Look for `Placing initial state in block n` in the log
* Start fill-postgresql with `--create`, `--skip-to n`, and `--trim`. Replace `n` with the value above.

If nodeos fails to receive blocks from the network, then try the above using `net_api_plugin`. Use `cleos net disconnect` and `cleos net connect` to reconnect nodes which timed out. Caution when using net_api_plugin: either use a firewall to block access to `http-server-address`, or change it to `localhost:8888`

Whenever you run fill-postgresql after this point, use the `--trim` option. Only use `--create` and `--skip-to` the first time.
