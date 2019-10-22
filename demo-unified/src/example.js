const { createClientWasm, transact } = require('./lib/eosio-client');
const fs = require('fs');
const { TextDecoder, TextEncoder } = require('util');
const fetch = require('node-fetch');
const encoder = new TextEncoder('utf8');
const decoder = new TextDecoder('utf8');
const { Api, JsonRpc, RpcError, Serialize } = require('eosjs');
const { JsSignatureProvider } = require('eosjs/dist/eosjs-jssig');

const signatureProvider = new JsSignatureProvider(['5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3']);
const rpc = new JsonRpc('http://127.0.0.1:8888', { fetch });
const api = new Api({ rpc, signatureProvider, textDecoder: new TextDecoder(), textEncoder: new TextEncoder() });

(async () => {
    try {
        const token = await createClientWasm({
            account: 'eosio.token',
            mod: new WebAssembly.Module(fs.readFileSync('eosio.token.client.wasm')),
            encoder, decoder
        });

        const result = await transact(api, {
            actions: [
                token.actions.create([{ actor: 'eosio.token', permission: 'active' }], 'eosio', '1000000.0000 TOK'),
                token.actions.issue([{ actor: 'eosio', permission: 'active' }], 'eosio', '1000000.0000 TOK', 'issuing'),
                token.actions.open([{ actor: 'a', permission: 'active' }], 'a', '4,TOK', 'a'),
                token.actions.open([{ actor: 'b', permission: 'active' }], 'b', '4,TOK', 'b'),
                token.actions.transfer([{ actor: 'eosio', permission: 'active' }], 'eosio', 'a', '100.0000 TOK', 'transfering'),
                token.actions.transfer([{ actor: 'a', permission: 'active' }], 'a', 'b', '100.0000 TOK', 'transfering'),
            ]
        }, {
            blocksBehind: 3,
            expireSeconds: 30,
        });
        console.log(result.processed.receipt.status);
    } catch (e) {
        if (e instanceof RpcError && e.json && e.json.error && e.json.error.details)
            console.log(e.json.error.details);
        else
            console.log(e);
    }
})()
