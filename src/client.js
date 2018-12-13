// copyright defined in LICENSE.txt

const fs = require('fs');
const { TextDecoder } = require('util');
const fetch = require('node-fetch');

const decoder = new TextDecoder('utf8');

class Foo {
    constructor() {
        let self = this;
        this.env = {
            abort() {
                throw new Error('called abort');
            },
            eosio_assert(test, msg) {
                if (!test)
                    throw new Error('assert failed');
            },
            get_blockchain_parameters_packed() {
                throw new Error('called get_blockchain_parameters_packed');
            },
            set_blockchain_parameters_packed() {
                throw new Error('called set_blockchain_parameters_packed');
            },
            print_range(begin, end) {
                process.stdout.write(decoder.decode(new Uint8Array(self.inst.exports.memory.buffer, begin, end - begin)));
            },
            set_result(begin, end) {
                self.result = Uint8Array.from(new Uint8Array(self.inst.exports.memory.buffer, begin, end - begin));
            },
        };

        const wasm = fs.readFileSync('./test-client.wasm');
        this.mod = new WebAssembly.Module(wasm);
        console.log('module created');
    }

    create_request() {
        this.inst = new WebAssembly.Instance(this.mod, { env: this.env });
        this.inst.exports.create_request();
        return this.result;
    }
}

let foo = new Foo();
let request = foo.create_request();

(async () => {
    try {
        console.log('aaaa');
        let x = await fetch('http://127.0.0.1:8080/wasmql/v1/query', { method: 'POST', body: request });
        if (x.status == 200) {
            console.log(await x.text());
        } else
            console.error(x.status, x.statusText);
    } catch (e) {
        console.error(e);
    }
})();
