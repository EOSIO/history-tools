# nodeos State History

## nodeos Configuration

| Option                                    | When to use |
|-------------------------------------------|-------------|
| `--plugin eosio::state_history_plugin`    | always |
| `--state-history-endpoint`                | optional; defaults to 127.0.0.1:8080 |
| `--trace-history`                         | enable to collect transaction and action traces *required for wasm-ql*  |
| `--chain-state-history`                   | enable to collect state (tables) *required for wasm-ql* |

Caution: either use a firewall to block access to the state-history endpoint, or leave it as `127.0.0.1:8080` to disable remote access.

## Fast start without history on existing chains

This option creates a database which tracks the chain state, but lacks most historical information.

* Get the following:
  * A portable snapshot (`data/snapshots/snapshot-xxxxxxx.bin`)
  * Optional: a block log which includes the block the snapshot was taken at
* Make sure `data/state` does not exist
* Start nodeos with the `--snapshot` option, and the options listed in "Nodeos configuration" above
* Look for `Placing initial state in block n` in the log
* Start a filler with `--fpg-create` (if PostgreSQL), `--fill-skip-to n`, and `--fill-trim`. Replace `n` with the value above.
* Do not stop nodeos until it has received at least 1 block from the network, or it won't be able to restart.

If nodeos fails to receive blocks from the network, then try the above using `net_api_plugin`. Use `cleos net disconnect` and `cleos net connect` to reconnect nodes which timed out. Caution when using net_api_plugin: either use a firewall to block access to `http-server-address`, or change it to `localhost:8888` to disable remote access.

Whenever you run a filler after this point, use the `--fill-trim` option. Only use `--fpg-create` and `--fill-skip-to` the first time.

## Creating a portable snapshot with full state-history

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

## Restoring a portable snapshot with full state-history

* Get the following:
  * A portable snapshot (`data/snapshots/snapshot-xxxxxxx.bin`)
  * The contents of `data/state-history`
  * Optional: a block log which includes the block the snapshot was taken at. Do not include `data/blocks/reversible`.
* Make sure `data/state` does not exist
* Start nodeos with the `--snapshot` option, and the options listed in "Nodeos configuration" above
* Do not stop nodeos until it has received at least 1 block from the network, or it won't be able to restart.

If nodeos fails to receive blocks from the network, then try the above using `net_api_plugin`. Use `cleos net disconnect` and `cleos net connect` to reconnect nodes which timed out. Caution when using net_api_plugin: either use a firewall to block access to `http-server-address`, or change it to `localhost:8888` to disable remote access.
