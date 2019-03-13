# wasm-ql

```
+----------+    +------------+    +---------------+       +-------------------+
| database |    | database   |    | wasm-ql       |       | web browser       |
| filler   |    |            |    | ------------- |       | ---------------   |
|          | => | postgresql | => | Server WASM A |  <=>  | Client WASM A     |
|          |    |     or     |    | Server WASM B |  <=>  | Client WASM B     |
|          |    |    lmdb    |    | ...           |       |                   |
|          |    |            |    | Legacy WASM   |  <=>  | js using /v1/ RPC |
+----------+    +------------+    +---------------+       +-------------------+
```

wasm-ql listens on an http port and answers the following:
* POST binary data to `http://host:port/wasmql/v1/query`
  * The binary data includes a set of queries directed to particular server WASMs
  * wasm-ql passes each query to the appropriate WASM
  * wasm-ql collects the query replies and produces a binary response containing the replies
* POST JSON query to `http://host:port/v1/*`
  * The legacy server WASM handles these requests
  * The WASM produces a JSON response
  * wasm-ql forwards the response to the client

Client WASMs provide these functions to js clients:
* `create_query_request()`: Convert a JSON request to the binary format the server WASM expects
* `decode_query_response()`: Convert a binary response from the server WASM to JSON
* `describe_query_request()`: Describes the JSON request format to clients using JSON Schema
* `describe_query_response()`: Describes the JSON response format to clients using JSON Schema
