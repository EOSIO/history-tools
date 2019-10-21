let { createClientWasm } = require('./lib/eosio-client');
const fs = require('fs');
const { TextDecoder, TextEncoder } = require('util');
const fetch = require('node-fetch');
const encoder = new TextEncoder('utf8');
const decoder = new TextDecoder('utf8');

(async () => {
    try {
        const token = await createClientWasm({
            mod: new WebAssembly.Module(fs.readFileSync('eosio.token.client.wasm')),
            encoder, decoder
        });
    } catch (e) {
        console.log(e);
    }
})()
