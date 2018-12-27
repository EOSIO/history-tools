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

    decode_response(reply) {
        this.input_data = new Uint8Array(reply);
        this.output_data = new Uint8Array(0);
        this.inst.exports.decode_response();
        // console.log('<<' + decoder.decode(this.output_data) + '>>')
        return JSON.parse(decoder.decode(this.output_data));
    }

    async round_trip(request) {
        const requestBin = this.create_request(request);
        const queryReply = await fetch('http://127.0.0.1:8080/wasmql/v1/query', { method: 'POST', body: requestBin });
        if (queryReply.status !== 200)
            throw new Error(queryReply.status + ': ' + queryReply.statusText + ': ' + await queryReply.text());
        return this.decode_response(await queryReply.arrayBuffer());
    }
}

function amount_to_decimal(amount) {
    let s;
    if (amount.amount[0] === '-')
        s = '-' + amount.amount.substr(1).padStart(amount.precision + 1, '0');
    else
        s = amount.amount.padStart(amount.precision + 1, '0');
    return s.substr(0, s.length - amount.precision) + '.' + s.substr(s.length - amount.precision);
}

function format_asset(amount, number_size = 18) {
    return amount_to_decimal(amount).padStart(number_size, ' ') + ' ' + amount.symbol;
}

function format_extended_asset(amount, number_size = 18) {
    return amount_to_decimal(amount).padStart(number_size, ' ') + ' ' + amount.symbol + '@' + amount.contract;
}

async function dump_block_info(clientWasm, first, last) {
    do {
        const reply = await clientWasm.round_trip(['block.info', {
            first, last,
            max_results: 100,
        }]);
        for (let block of reply[1].blocks)
            console.log(JSON.stringify(block, null, 4));
        first = reply[1].more;
    } while (first);
}

async function dump_tapos(clientWasm, ref_block, expire_seconds) {
    const reply = await clientWasm.round_trip(['tapos', {
        ref_block,
        expire_seconds,
    }]);
    console.log(JSON.stringify(reply, null, 4));
}

async function dump_eos_balances(clientWasm, first_account, last_account) {
    do {
        const reply = await clientWasm.round_trip(['bal.mult.acc', {
            max_block: ["absolute", 30000000],
            code: 'eosio.token',
            sym: 'EOS',
            first_account: first_account,
            last_account: last_account,
            max_results: 100,
        }]);
        for (let balance of reply[1].balances)
            console.log(balance.account.padEnd(13, ' '), format_extended_asset(balance.amount));
        first_account = reply[1].more;
    } while (first_account);
}

async function dump_tokens(clientWasm, first_key, last_key) {
    do {
        const reply = await clientWasm.round_trip(['bal.mult.tok', {
            max_block: ["head", 0],
            account: 'b1',
            first_key,
            last_key,
            max_results: 100,
        }]);
        for (let balance of reply[1].balances)
            console.log(balance.account.padEnd(13, ' '), format_extended_asset(balance.amount));
        first_key = reply[1].more;
    } while (first_key);
}

async function dump_transfers(clientWasm) {
    let first_key = {
        receipt_receiver: 'eosio.bpay',
        account: 'eosio.token',
        block_index: 0,
        transaction_id: '0000000000000000000000000000000000000000000000000000000000000000',
        action_index: 0,
    };
    let last_key = {
        receipt_receiver: 'eosio.bpay',
        account: 'eosio.token',
        block_index: 0xffffffff,
        transaction_id: 'ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff',
        action_index: 0xffffffff,
    };

    let i = 0;
    while (first_key) {
        const reply = await clientWasm.round_trip(['transfer', {
            max_block: ["irreversible", 0],
            first_key,
            last_key,
            max_results: 10,
            include_notify_incoming: true,
            include_notify_outgoing: true,
        }]);
        for (let transfer of reply[1].transfers)
            console.log(
                transfer.from.padEnd(13, ' ') + ' -> ' + transfer.to.padEnd(13, ' '),
                format_extended_asset(transfer.quantity), '     ', transfer.memo);
        first_key = reply[1].more;
        i += reply[1].transfers.length;
        console.log(i);
        if (i >= 20)
            break;
    }
}

(async () => {
    try {
        const clientWasm = new ClientWasm();
        await dump_block_info(clientWasm, 30000000, 30000001);
        console.log();
        await dump_tapos(clientWasm, 30000000, 0);
        console.log();
        await dump_eos_balances(clientWasm, 'eosio', 'eosio.zzzzzzj');
        console.log();
        await dump_tokens(clientWasm, { sym: '', code: '' }, { sym: 'ZZZZZZZ', code: 'zzzzzzzzzzzzj' });
        console.log();
        await dump_transfers(clientWasm);
    } catch (e) {
        console.error(e);
    }
})();
