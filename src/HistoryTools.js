// copyright defined in LICENSE.txt

'use strict';

(function (exports) {
    /** Manage a client-side WASM */
    class ClientWasm {
        /** 
         * @param {WebAssembly.Module} mod - module containing the WASM
         * @param {TextEncoder} encoder - utf8 text encoder
         * @param {TextDecoder} decoder - utf8 text decoder
         */
        constructor({ mod, encoder, decoder }) {
            const self = this;
            this.mod = mod;
            this.encoder = encoder;
            this.decoder = decoder;
            this.input_data = new Uint8Array(0);

            // Primitives exposed to the WASM module
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

        /** Instantiate the WASM module. Creates a new `WebAssembly.Instance` with fresh memory. */
        async instantiate() {
            this.inst = await WebAssembly.instantiate(this.mod, { env: this.primitives });
            this.inst.exports.initialize();
        }

        /** Returns a JSON Schema describing requests that `createQueryRequest` accepts */
        describeQueryRequest() {
            this.inst.exports.describe_query_request();
            return this.decoder.decode(this.output_data);
        }

        /** Returns a JSON Schema describing responses that `createQueryRequest` returns */
        describeQueryResponse() {
            this.inst.exports.describe_query_response();
            return this.decoder.decode(this.output_data);
        }

        /** 
         * Converts `request` to the binary format that the server-side WASM expects. `request`
         * must be a string containing JSON which matches the schema returned by `describeQueryRequest`.
         */
        createQueryRequest(request) {
            this.input_data = this.encoder.encode(request);
            this.output_data = new Uint8Array(0);
            this.inst.exports.create_query_request();
            return this.output_data;
        }

        /**
         * Converts binary response from a server-side WASM to JSON. The format matches
         * the schema returned by `describeQueryResponse`.
         */
        decodeQueryResponse(reply) {
            this.input_data = new Uint8Array(reply);
            this.output_data = new Uint8Array(0);
            this.inst.exports.decode_query_response();
            return this.decoder.decode(this.output_data);
        }
    } // ClientWasm

    exports.ClientWasm = ClientWasm;

    /** 
     * Create a `ClientWasm` and instantiate the module
     * @param {WebAssembly.Module} mod - module containing the WASM
     * @param {TextEncoder} encoder - utf8 text encoder
     * @param {TextDecoder} decoder - utf8 text decoder
     */
    async function createClientWasm({ mod, encoder, decoder }) {
        const result = new ClientWasm({ mod, encoder, decoder });
        await result.instantiate();
        return result;
    }

    exports.createClientWasm = createClientWasm;

    /** Combine multiple binary requests into the format the wasm-ql server needs. Returns a Uint8Array. */
    function combineRequests(requests) {
        const buf = new SerialBuffer;
        buf.pushVaruint32(requests.length);
        for (let request of requests)
            buf.pushBytes(request);
        return buf.asUint8Array();
    }

    exports.combineRequests = combineRequests;

    function splitResponses(responses) {
        const buf = new SerialBuffer({ array: responses });
        const n = buf.getVaruint32();
        const result = [];
        for (let i = 0; i < n; ++i)
            result.push(buf.getBytes());
        return result;
    }

    exports.splitResponses = splitResponses;

    /** Serialize and deserialize data. This is a subset of eosjs's SerialBuffer. */
    class SerialBuffer {
        constructor({ array } = {}) {
            this.array = array || new Uint8Array(1024);
            this.length = array ? array.length : 0;
            this.readPos = 0;
        }

        /** Resize `array` if needed to have at least `size` bytes free */
        reserve(size) {
            if (this.length + size <= this.array.length) {
                return;
            }
            let l = this.array.length;
            while (this.length + size > l) {
                l = Math.ceil(l * 1.5);
            }
            const newArray = new Uint8Array(l);
            newArray.set(this.array);
            this.array = newArray;
        }

        /** Return data with excess storage trimmed away */
        asUint8Array() {
            return new Uint8Array(this.array.buffer, this.array.byteOffset, this.length);
        }

        /** Append bytes */
        pushArray(v) {
            this.reserve(v.length);
            this.array.set(v, this.length);
            this.length += v.length;
        }

        /** Get a single byte */
        get() {
            if (this.readPos < this.length) {
                return this.array[this.readPos++];
            }
            throw new Error("Read past end of buffer");
        }

        /** Append a `varuint32` */
        pushVaruint32(v) {
            while (true) {
                if (v >>> 7) {
                    this.pushArray([0x80 | (v & 0x7f)]);
                    v = v >>> 7;
                } else {
                    this.pushArray([v]);
                    break;
                }
            }
        }

        /** Get a `varuint32` */
        getVaruint32() {
            let v = 0;
            let bit = 0;
            while (true) {
                const b = this.get();
                v |= (b & 0x7f) << bit;
                bit += 7;
                if (!(b & 0x80)) {
                    break;
                }
            }
            return v >>> 0;
        }

        /** Get `len` bytes */
        getUint8Array(len) {
            if (this.readPos + len > this.length)
                throw new Error("Read past end of buffer");
            const result = new Uint8Array(this.array.buffer, this.array.byteOffset + this.readPos, len);
            this.readPos += len;
            return result;
        }

        /** Append length-prefixed binary data */
        pushBytes(v) {
            this.pushVaruint32(v.length);
            this.pushArray(v);
        }

        /** Get length-prefixed binary data */
        getBytes() {
            return this.getUint8Array(this.getVaruint32());
        }
    } // SerialBuffer

    exports.SerialBuffer = SerialBuffer;

})(typeof exports === 'undefined' ? this['HistoryTools'] = {} : exports);
