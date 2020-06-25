## Classes

_[ClientWasm](./HistoryTools_js#ClientWasm)_

Manage a client-side WASM

_[SerialBuffer](./HistoryTools_js#SerialBuffer)_

Serialize and deserialize data. This is a subset of eosjs's SerialBuffer.


## ClientWasm

Manage a client-side WASM

Kind: global class



*   [ClientWasm](./HistoryTools_js#ClientWasm)
    *   [new ClientWasm(mod, encoder, decoder)](./HistoryTools_js#new_ClientWasm_new)
    *   [.instantiate()](./HistoryTools_js#ClientWasm+instantiate)
    *   [.describeQueryRequest()](./HistoryTools_js#ClientWasm+describeQueryRequest)
    *   [.describeQueryResponse()](./HistoryTools_js#ClientWasm+describeQueryResponse)
    *   [.createQueryRequest()](./HistoryTools_js#ClientWasm+createQueryRequest)
    *   [.decodeQueryResponse()](./HistoryTools_js#ClientWasm+decodeQueryResponse)


### new ClientWasm(mod, encoder, decoder)


<table>
  <tr>
   <td><strong>Param</strong>
   </td>
   <td><strong>Type</strong>
   </td>
   <td><strong>Description</strong>
   </td>
  </tr>
  <tr>
   <td>mod
   </td>
   <td><code>WebAssembly.Module</code>
   </td>
   <td>module containing the WASM
   </td>
  </tr>
  <tr>
   <td>encoder
   </td>
   <td><code>TextEncoder</code>
   </td>
   <td>utf8 text encoder
   </td>
  </tr>
  <tr>
   <td>decoder
   </td>
   <td><code>TextDecoder</code>
   </td>
   <td>utf8 text decoder
   </td>
  </tr>
</table>



### clientWasm.instantiate()

Instantiate the WASM module. Creates a new `WebAssembly.Instance` with fresh memory.

Kind: instance method of <code>[ClientWasm](./HistoryTools_js#ClientWasm)</code>


### clientWasm.describeQueryRequest()

Returns a JSON Schema describing requests that `createQueryRequest` accepts

Kind: instance method of <code>[ClientWasm](./HistoryTools_js#ClientWasm)</code>


### clientWasm.describeQueryResponse()

Returns a JSON Schema describing responses that `createQueryRequest` returns

Kind: instance method of <code>[ClientWasm](./HistoryTools_js#ClientWasm)</code>


### clientWasm.createQueryRequest()

Converts `request` to the binary format that the server-side WASM expects. `request` must be a string containing JSON which matches the schema returned by `describeQueryRequest`.

Kind: instance method of <code>[ClientWasm](./HistoryTools_js#ClientWasm)</code>


### clientWasm.decodeQueryResponse()

Converts binary response from a server-side WASM to JSON. The format matches the schema returned by `describeQueryResponse`.

Kind: instance method of <code>[ClientWasm](./HistoryTools_js#ClientWasm)</code>


## SerialBuffer

Serialize and deserialize data. This is a subset of eosjs's SerialBuffer.

Kind: global class



*   [SerialBuffer](./HistoryTools_js#SerialBuffer)
    *   [.reserve()](./HistoryTools_js#SerialBuffer+reserve)
    *   [.asUint8Array()](./HistoryTools_js#SerialBuffer+asUint8Array)
    *   [.pushArray()](./HistoryTools_js#SerialBuffer+pushArray)
    *   [.get()](./HistoryTools_js#SerialBuffer+get)
    *   [.pushVaruint32()](./HistoryTools_js#SerialBuffer+pushVaruint32)
    *   [.getVaruint32()](./HistoryTools_js#SerialBuffer+getVaruint32)
    *   [.getUint8Array()](./HistoryTools_js#SerialBuffer+getUint8Array)
    *   [.pushBytes()](./HistoryTools_js#SerialBuffer+pushBytes)
    *   [.getBytes()](./HistoryTools_js#SerialBuffer+getBytes)


### serialBuffer.reserve()

Resize `array` if needed to have at least `size` bytes free

Kind: instance method of <code>[SerialBuffer](./HistoryTools_js#SerialBuffer)</code>


### serialBuffer.asUint8Array()

Return data with excess storage trimmed away

Kind: instance method of <code>[SerialBuffer](./HistoryTools_js#SerialBuffer)</code>


### serialBuffer.pushArray()

Append bytes

Kind: instance method of <code>[SerialBuffer](./HistoryTools_js#SerialBuffer)</code>


### serialBuffer.get()

Get a single byte

Kind: instance method of <code>[SerialBuffer](./HistoryTools_js#SerialBuffer)</code>


### serialBuffer.pushVaruint32()

Append a `varuint32`

Kind: instance method of <code>[SerialBuffer](./HistoryTools_js#SerialBuffer)</code>


### serialBuffer.getVaruint32()

Get a `varuint32`

Kind: instance method of <code>[SerialBuffer](./HistoryTools_js#SerialBuffer)</code>


### serialBuffer.getUint8Array()

Get `len` bytes

Kind: instance method of <code>[SerialBuffer](./HistoryTools_js#SerialBuffer)</code>


### serialBuffer.pushBytes()

Append length-prefixed binary data

Kind: instance method of <code>[SerialBuffer](./HistoryTools_js#SerialBuffer)</code>


### serialBuffer.getBytes()

Get length-prefixed binary data

Kind: instance method of <code>[SerialBuffer](./HistoryTools_js#SerialBuffer)</code>
