'use strict';

const { Serialize } = require('eosjs');

(function (exports) {
    class ClientWasm {
        constructor({ mod, encoder, decoder, account, fetch, queryUrl }) {
            const self = this;
            this.mod = mod;
            this.encoder = encoder;
            this.decoder = decoder;
            this.account = account;
            this.fetch = fetch;
            this.queryUrl = queryUrl;
            this.input_data = new Uint8Array(0);

            this.primitives = {
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
                print_range(begin, end) {
                    if (begin !== end)
                        process.stdout.write(decoder.decode(new Uint8Array(self.inst.exports.memory.buffer, begin, end - begin)));
                },
                get_input_data(ptr, size) {
                    const input_data = self.input_data;
                    if (!size)
                        return input_data.length;
                    const copy_size = Math.min(input_data.length, size);
                    const dest = new Uint8Array(self.inst.exports.memory.buffer, ptr, copy_size);
                    for (let i = 0; i < copy_size; ++i)
                        dest[i] = input_data[i];
                    return copy_size;
                },
                set_output_data(ptr, size) {
                    self.output_data = Uint8Array.from(new Uint8Array(self.inst.exports.memory.buffer, ptr, size));
                },
            };
        }

        getZStr(addr) {
            let buf = new Uint8Array(this.inst.exports.memory.buffer, addr);
            let len = 0;
            while (buf[len] !== 0)
                ++len;
            return this.decoder.decode(new Uint8Array(this.inst.exports.memory.buffer, addr, len));
        }

        action_to_bin(action, ...args) {
            this.input_data = this.encoder.encode(JSON.stringify([action, ...args]));
            this.inst.exports.action_to_bin();
            return this.output_data;
        }

        query_to_bin(query, ...args) {
            this.input_data = this.encoder.encode(JSON.stringify([query, ...args]));
            this.inst.exports.query_to_bin();
            return this.output_data;
        }

        query_result_to_json(result) {
            this.input_data = result;
            this.inst.exports.query_result_to_json();
            return this.decoder.decode(this.output_data);
        }

        async query(account, query, ...args) {
            const response = await this.fetch(this.queryUrl + '/wasmql/v2/query/' + account + '/' + query, {
                body: this.query_to_bin(query, ...args),
                method: 'POST',
            });
            if (!response.ok)
                throw new Error(response.status + ': ' + response.statusText + ': ' + await response.text());
            const buf = await response.arrayBuffer();
            return this.query_result_to_json(new Uint8Array(buf));
        }

        async instantiate() {
            this.inst = await WebAssembly.instantiate(this.mod, { env: this.primitives });
            this.inst.exports.initialize();
            this.actions = {};
            const actions = JSON.parse(this.getZStr(this.inst.exports.get_actions()));
            for (let action of actions) {
                this.actions[action.name] = (authorization, ...args) => ({
                    account: this.account,
                    name: action.name,
                    authorization,
                    data: this.action_to_bin(action.name, ...args),
                });
            }

            this.queries = {};
            const queries = JSON.parse(this.getZStr(this.inst.exports.get_queries()));
            for (let query of queries)
                this.queries[query.name] = (...args) => this.query(this.account, query.name, ...args);
        }
    } // ClientWasm
    exports.ClientWasm = ClientWasm;

    async function createClientWasm(args) {
        const result = new ClientWasm(args);
        await result.instantiate();
        return result;
    }
    exports.createClientWasm = createClientWasm;

    async function transact(api, transaction, { broadcast = true, sign = true, blocksBehind, expireSeconds } = {}) {
        let info;

        if (!api.chainId) {
            info = await api.rpc.get_info();
            api.chainId = info.chain_id;
        }

        if (typeof blocksBehind === 'number' && expireSeconds) {
            if (!info) {
                info = await api.rpc.get_info();
            }
            const refBlock = await api.rpc.get_block(info.head_block_num - blocksBehind);
            transaction = { ...Serialize.transactionHeader(refBlock, expireSeconds), ...transaction };
        }

        if (!api.hasRequiredTaposFields(transaction)) {
            throw new Error('Required configuration or TAPOS fields are not present');
        }

        const serializedTransaction = api.serializeTransaction(transaction);
        let pushTransactionArgs = { serializedTransaction, signatures: [] };

        if (sign) {
            const requiredKeys = ['EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV'];
            pushTransactionArgs = await api.signatureProvider.sign({
                chainId: api.chainId,
                requiredKeys,
                serializedTransaction,
                abis: {},
            });
        }
        if (broadcast) {
            return api.rpc.send_transaction(pushTransactionArgs);
        }
        return pushTransactionArgs;
    }
    exports.transact = transact;
})(typeof exports === 'undefined' ? this['eosio-client'] = {} : exports);
