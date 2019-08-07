# Using the client WASMs

The client WASMs convert queries and query results between JSON and binary form. Usage:

* Load the client WASMs during app initialization
* Use the client WASMs to convert 1 or more queries to binary
* Combine the binary queries into a single binary
* POST the query binary to `http(s)://server.name:port/wasmql/v1/query`. The response is a binary.
* Split the binary response into multiple responses
* Use the client WASMs to convert them to JSON

Different client WASMs handle different kinds of queries. e.g. `chain-client.wasm` can handle queries about accounts and `token-client.wasm` can handle queries about tokens.

# HistoryTools.js

`HistoryTools.js` helps browser apps and nodejs servers use client WASMs.

## Using in web browsers

This code snippet uses `HistoryTools.js` to load some client WASMs:

```html
<script src=".../path/to/HistoryTools.js"></script>

<script>
    const encoder = new TextEncoder('utf8');
    const decoder = new TextDecoder('utf8');

    function prettyPrint(title, json) {
        console.log('\n' + title + '\n====================');
        console.log(JSON.stringify(JSON.parse(json), null, 4));
    }

    (async () => {
        try {
            const chainClientWasm = await HistoryTools.createClientWasm({
                mod: await WebAssembly.compileStreaming(fetch('.../path/to/chain-client.wasm')),
                encoder, decoder
            });

            const tokenClientWasm = await HistoryTools.createClientWasm({
                mod: await WebAssembly.compileStreaming(fetch('.../path/to/token-client.wasm')),
                encoder, decoder
            });

            // ... insert code examples from below here ...

        } catch (e) {
            console.log(e);
        }
    })()
</script>
```

## Using with nodejs

This code snippet uses `HistoryTools.js` to load some client WASMs:

```js
const HistoryTools = require('.../path/to/HistoryTools.js');
const fs = require('fs');
const { TextDecoder, TextEncoder } = require('util');
const fetch = require('node-fetch');
const encoder = new TextEncoder('utf8');
const decoder = new TextDecoder('utf8');

function prettyPrint(title, json) {
    console.log('\n' + title + '\n====================');
    console.log(JSON.stringify(JSON.parse(json), null, 4));
}

(async () => {
    try {
        const chainClientWasm = await HistoryTools.createClientWasm({
            mod: new WebAssembly.Module(fs.readFileSync('.../path/to/chain-client.wasm')),
            encoder, decoder
        });

        const tokenClientWasm = await HistoryTools.createClientWasm({
            mod: new WebAssembly.Module(fs.readFileSync('.../path/to/token-client.wasm')),
            encoder, decoder
        });

        // ... insert code examples from below here ...

    } catch (e) {
        console.log(e);
    }
})()
```

## Examining schemas

The `describeQueryRequest` and `describeQueryResponse` methods return JSON schemas:

```js
prettyPrint('Token Request Schema', tokenClientWasm.describeQueryRequest());
prettyPrint('Token Response Schema', tokenClientWasm.describeQueryResponse());
```

## Creating query requests

The `createQueryRequest` method converts query requests from JSON to binary:

```js
// Create a request: generate TAPOS fields needed for a transaction
const request0 = chainClientWasm.createQueryRequest(JSON.stringify(
    ['tapos', {
        ref_block: ['head', -10],    // 10 blocks behind head
        expire_seconds: 60,          // Expire 60 seconds after that block
    }]
));

// Create a request: get a range of accounts
const request1 = chainClientWasm.createQueryRequest(JSON.stringify(
    ['account', {
        snapshot_block: ['head', 0],
        first: 'eosio',
        last: 'eosio.zzzzzz',
        max_results: 100,
        include_code: false,
        include_abi: false,
    }]
));

// Create a request: get tokens (from multiple contracts) owned by `eosio`
const request2 = tokenClientWasm.createQueryRequest(JSON.stringify(
    ['bal.mult.tok', {
        snapshot_block: ['head', 0],
        account: 'eosio',
        first_key: { sym: '', code: '' },
        last_key: { sym: 'ZZZZZZZ', code: 'zzzzzzzzzzzzj' },
        max_results: 10,
    }]
));
```

## Combining the request, sending it, and splitting the response.

The `combineRequests` function combines multiple requests into a single wasm-ql request. The
`splitResponses` function splits up multiple replies from a single binary. Both functions
are required even when there's only 1 request.

```js
// Query wasm-ql server
const queryReply = await fetch('http(s)://server.name:port/wasmql/v1/query', {
    method: 'POST',
    body: HistoryTools.combineRequests([
        request0,
        request1,
        request2
    ]),
});
if (queryReply.status !== 200)
    throw new Error(queryReply.status + ': ' + queryReply.statusText + ': ' + await queryReply.text());

// Split up responses
const responses = HistoryTools.splitResponses(new Uint8Array(await queryReply.arrayBuffer()));
```

## Converting responses to JSON

The `decodeQueryResponse` method converts responses from binary to JSON. Call it 
from the WASM which generated the matching query.

```js
// Look at responses
prettyPrint('tapos', chainClientWasm.decodeQueryResponse(responses[0]));
prettyPrint('accounts', chainClientWasm.decodeQueryResponse(responses[1]));
prettyPrint('tokens', tokenClientWasm.decodeQueryResponse(responses[2]));
```
