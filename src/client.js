// copyright defined in LICENSE.txt

const fs = require('fs');
const { TextEncoder, TextDecoder } = require('util');
const fetch = require('node-fetch');

const encoder = new TextEncoder('utf8');
const decoder = new TextDecoder('utf8');

class ClientWasm {
    constructor() {
        const self = this;
        this.env = {
            abort() {
                throw new Error('called abort');
            },
            eosio_assert_message(test, begin, len) {
                if (!test) {
                    let e;
                    try {
                        e = new Error('assert failed with message: ' + decoder.decode(new Uint8Array(self.inst.exports.memory.buffer, begin, len)));
                    }
                    catch (x) {
                        e = new Error('assert failed');
                    }
                    throw e;
                }
            },
            get_blockchain_parameters_packed() {
                throw new Error('called get_blockchain_parameters_packed');
            },
            set_blockchain_parameters_packed() {
                throw new Error('called set_blockchain_parameters_packed');
            },
            print_range(begin, end) {
                if (begin !== end)
                    process.stdout.write(decoder.decode(new Uint8Array(self.inst.exports.memory.buffer, begin, end - begin)));
            },
            get_input_data(cb_alloc_data, cb_alloc) {
                const input_data = self.input_data;
                const ptr = self.inst.exports.__indirect_function_table.get(cb_alloc)(cb_alloc_data, input_data.length);
                const dest = new Uint8Array(self.inst.exports.memory.buffer, ptr, input_data.length);
                for (let i = 0; i < input_data.length; ++i)
                    dest[i] = input_data[i];
            },
            set_output_data(begin, end) {
                self.output_data = Uint8Array.from(new Uint8Array(self.inst.exports.memory.buffer, begin, end - begin));
            },
        };

        const wasm = fs.readFileSync('./test-client.wasm');
        this.input_data = new Uint8Array(0);
        this.mod = new WebAssembly.Module(wasm);
        this.reset();
    }

    reset() {
        this.inst = new WebAssembly.Instance(this.mod, { env: this.env });
    }

    create_request(request) {
        this.input_data = encoder.encode(JSON.stringify(request));
        this.output_data = new Uint8Array(0);
        this.inst.exports.create_request();
        return this.output_data;
    }

    decode_reply(reply) {
        this.input_data = reply;
        this.output_data = new Uint8Array(0);
        this.inst.exports.decode_reply();
        return this.output_data;
    }
}

const clientWasm = new ClientWasm();
const request = clientWasm.create_request({
    max_block_index: 100000000,
    code: 'eosio.token',
    sym: 'EOS',
    first_account: 'monster',
    last_account: 'zzzzzzzzzzzzj',
    max_results: 10,
});

(async () => {
    try {
        const queryReply = await fetch('http://127.0.0.1:8080/wasmql/v1/query', { method: 'POST', body: request });
        if (queryReply.status === 200) {
            const a = clientWasm.decode_reply(new Uint8Array(await queryReply.arrayBuffer()));
            const b = decoder.decode(a);
            const c = JSON.parse(b);
            console.log(JSON.stringify(c, null, 4));
        } else
            console.error(queryReply.status, queryReply.statusText);
    } catch (e) {
        console.error(e);
    }
})();
