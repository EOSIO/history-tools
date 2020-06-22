---
content_title: Containerized Demos
---

These self-contained demonstrations run in transient containers.

## Talk

This demo stores conversations on chain and uses wasm-ql to serve the messages to clients 
in threaded order. The UI requests messages as needed to fill a virtual scroll area. The
UI also has pages describing how the demo is constructed and reveals the sources for the
contract and wasm-ql wasms.

The single container hosts `nodeos`, `combo-rocksdb`, `nginx`, a background transaction
pusher, and the UI.

To start the demo, run the following, then point your browser to http://127.0.0.1:8881

```
docker pull eosio/history-tools:demo-talk
docker run --rm -it -p 127.0.0.1:8881:80 eosio/history-tools:demo-talk
```

## Partial History

Here's a demonstration of using wasm-ql's chain and token queries. This docker container
is populated with partial history data drawn from one of the public EOSIO networks.
It uses RocksDB.

To start the demo, run the following, then point your browser to http://127.0.0.1:8882

```
docker pull eosio/history-tools:demo-gui-snapshot
docker run --rm -it -p 127.0.0.1:8882:80 eosio/history-tools:demo-gui-snapshot
```
