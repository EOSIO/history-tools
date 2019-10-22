'use strict';

const { Serialize } = require('eosjs');

(function (exports) {
    class ClientWasm {
        constructor({ mod, encoder, decoder, account }) {
            const self = this;
            this.mod = mod;
            this.encoder = encoder;
            this.decoder = decoder;
            this.account = account;
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
        }
    } // ClientWasm
    exports.ClientWasm = ClientWasm;

    async function createClientWasm({ mod, encoder, decoder, account }) {
        const result = new ClientWasm({ mod, encoder, decoder, account });
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

        const abis = await api.getTransactionAbis(transaction);
        const serializedTransaction = api.serializeTransaction(transaction);
        let pushTransactionArgs = { serializedTransaction, signatures: [] };

        if (sign) {
            const requiredKeys = ['EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV'];
            pushTransactionArgs = await api.signatureProvider.sign({
                chainId: api.chainId,
                requiredKeys,
                serializedTransaction,
                abis,
            });
        }
        if (broadcast) {
            return api.pushSignedTransaction(pushTransactionArgs);
        }
        return pushTransactionArgs;
    }
    exports.transact = transact;

})(typeof exports === 'undefined' ? this['eosio-client'] = {} : exports);
