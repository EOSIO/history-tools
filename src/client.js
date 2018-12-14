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
        this.input_data = new Uint8Array(reply);
        this.output_data = new Uint8Array(0);
        this.inst.exports.decode_reply();
        return JSON.parse(decoder.decode(this.output_data));
    }

    async round_trip(request) {
        const requestBin = this.create_request(request);
        const queryReply = await fetch('http://127.0.0.1:8080/wasmql/v1/query', { method: 'POST', body: requestBin });
        if (queryReply.status !== 200)
            throw new Error(queryReply.status + ": " + queryReply.statusText);
        return this.decode_reply(await queryReply.arrayBuffer());
    }
}

async function dump_eos_balances(clientWasm, first_account, last_account) {
    do {
        const reply = await clientWasm.round_trip(['bal.mult.acc', {
            max_block_index: 100000000,
            code: 'eosio.token',
            sym: 'EOS',
            first_account: first_account,
            last_account: last_account,
            max_results: 100,
        }]);
        for (let row of reply.rows)
            console.log(
                row.account.padEnd(13, ' '),
                row.amount.amount.padStart(18, ' '),
                row.amount.symbol + '@' + row.amount.contract);
        first_account = reply.more;
    } while (first_account);
}

async function dump_tokens(clientWasm, first_key, last_key) {
    do {
        const reply = await clientWasm.round_trip(['bal.mult.tok', {
            max_block_index: 100000000,
            account: 'b1',
            first_key,
            last_key,
            max_results: 100,
        }]);
        for (let row of reply.rows)
            console.log(
                row.account.padEnd(13, ' '),
                row.amount.amount.padStart(18, ' '),
                row.amount.symbol + '@' + row.amount.contract);
        first_key = reply.more;
    } while (first_key);
}

(async () => {
    try {
        const clientWasm = new ClientWasm();
        await dump_eos_balances(clientWasm, 'eosio', 'eosio.zzzzzzj');
        console.log();
        await dump_tokens(clientWasm, { sym: '', code: '' }, { sym: 'ZZZZZZZ', code: 'zzzzzzzzzzzzj' });
    } catch (e) {
        console.error(e);
    }
})();
