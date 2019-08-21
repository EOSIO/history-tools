## Introduction

This demonstrates using `combo-rocksdb` to record and query threaded conversations stored on chain. It has the following components running in a container:

| Component             | Description |
| ---------------       | ----------- |
| `nodeos`              | Runs a test chain. Includes the State-History Plugin. |
| `combo-rocksdb`       | Receives history data from the State-History plugin and populates a rocksdb database. Also processes queries on the rocksdb database. |
| `talk.wasm`           | This contract accepts `post` actions and records the messages in a table. |
| `talk-server.wasm`    | This wasm processes queries on the messages. It runs within `combo-rocksdb`. |
| `talk-client.wasm`    | This wasm runs in the web browser. It converts queries from JSON to binary form and responses from binary form to JSON. |
| `ClientRoot.tsx`      | This is the UI which runs in the web browser. |
| `fill.js`             | This sends transactions containing random messages to the chain. |
| `nginx`               | This serves static files, proxies nodeos's `/v1/` API, and proxies `combo-rocksdb`'s `/wasmql/` API. All these services appear on a common HTTP port. |
| `supervisord`         | Spawns the `nodeos`, `combo-rocksdb`, `fill.js`, and `nginx` processes. |

## Using this demo

* Select the **Messages** radio button to see the threaded messages from the chain. Use the
  **Query Inspector** and **Reply Inspector** to see the the JSON data going into and
  coming from `talk-client.wasm`.
* Select the **Accounts** radio button to see the list of accounts on chain.
* Select the other radio buttons to read more explanations and see source code.
