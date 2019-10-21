'use strict';

(function (exports) {
    class ClientWasm {
        constructor({ mod, encoder, decoder }) {
            const self = this;
            this.mod = mod;
            this.encoder = encoder;
            this.decoder = decoder;
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

        async instantiate() {
            this.inst = await WebAssembly.instantiate(this.mod, { env: this.primitives });
            this.inst.exports.initialize();
            console.log('...', this.inst.exports.get_actions(), '...');
            console.log('...', this.getZStr(this.inst.exports.get_actions()), '...');
        }
    } // ClientWasm
    exports.ClientWasm = ClientWasm;

    async function createClientWasm({ mod, encoder, decoder }) {
        const result = new ClientWasm({ mod, encoder, decoder });
        await result.instantiate();
        return result;
    }
    exports.createClientWasm = createClientWasm;
})(typeof exports === 'undefined' ? this['eosio-client'] = {} : exports);
