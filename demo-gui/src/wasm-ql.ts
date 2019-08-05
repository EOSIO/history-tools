// copyright defined in LICENSE.txt

const encoder = new TextEncoder();
const decoder = new TextDecoder();

class SerialBuffer {
    public array: Uint8Array;
    public length: number;
    public readPos: number;

    constructor({ array }: { array?: Uint8Array } = {}) {
        this.array = array || new Uint8Array(1024);
        this.length = array ? array.length : 0;
        this.readPos = 0;
    }

    public reserve(size) {
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

    public asUint8Array() {
        return new Uint8Array(this.array.buffer, this.array.byteOffset, this.length);
    }

    public pushArray(v) {
        this.reserve(v.length);
        this.array.set(v, this.length);
        this.length += v.length;
    }

    public get() {
        if (this.readPos < this.length) {
            return this.array[this.readPos++];
        }
        throw new Error('Read past end of buffer');
    }

    public pushVaruint32(v: number) {
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

    public getVaruint32() {
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

    public getUint8Array(len: number) {
        if (this.readPos + len > this.length)
            throw new Error('Read past end of buffer');
        const result = new Uint8Array(this.array.buffer, this.array.byteOffset + this.readPos, len);
        this.readPos += len;
        return result;
    }

    public pushBytes(v) {
        this.pushVaruint32(v.length);
        this.pushArray(v);
    }

    public getBytes() {
        return this.getUint8Array(this.getVaruint32());
    }
} // SerialBuffer

export class ClientWasm {
    public env: any;
    public open_promise: Promise<void>;
    public inst: WebAssembly.Instance;
    public input_data: Uint8Array;
    public output_data: Uint8Array;

    constructor(path: string) {
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
                    } catch (x) {
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

        this.input_data = new Uint8Array(0);
        this.open_promise = (WebAssembly as any).instantiateStreaming(fetch(path), { env: this.env }).then(x => {
            this.inst = x.instance;
            this.inst.exports.initialize();
        });
    }

    public describe_query_request() {
        this.inst.exports.describe_query_request();
        return JSON.parse(decoder.decode(this.output_data));
    }

    public describe_query_response() {
        this.inst.exports.describe_query_response();
        return JSON.parse(decoder.decode(this.output_data));
    }

    public create_query_request(request) {
        this.input_data = encoder.encode(JSON.stringify(request));
        this.output_data = new Uint8Array(0);
        this.inst.exports.create_query_request();
        return this.output_data;
    }

    public decode_query_response(reply) {
        this.input_data = new Uint8Array(reply);
        this.output_data = new Uint8Array(0);
        this.inst.exports.decode_query_response();
        // console.log('<<' + decoder.decode(this.output_data) + '>>')
        return JSON.parse(decoder.decode(this.output_data));
    }

    public async round_trip(request) {
        await this.open_promise;
        const requestBin = this.create_query_request(request);
        const bin = new SerialBuffer();
        bin.pushVaruint32(1);
        bin.pushBytes(requestBin);
        const queryReply = await fetch('/wasmql/v1/query', { method: 'POST', body: bin.asUint8Array() });
        if (queryReply.status !== 200)
            throw new Error(queryReply.status + ': ' + queryReply.statusText + ': ' + await queryReply.text());
        const reply = new SerialBuffer({ array: new Uint8Array(await queryReply.arrayBuffer()) });
        if (reply.getVaruint32() !== 1)
            throw new Error('expected 1 reply');
        return this.decode_query_response(reply.getBytes());
    }
} // ClientWasm

export function amount_to_decimal(amount) {
    let s;
    if (amount.amount[0] === '-')
        s = '-' + amount.amount.substr(1).padStart(amount.precision + 1, '0');
    else
        s = amount.amount.padStart(amount.precision + 1, '0');
    return s.substr(0, s.length - amount.precision) + '.' + s.substr(s.length - amount.precision);
}

export function format_asset(amount, number_size = 18) {
    return amount_to_decimal(amount).padStart(number_size, ' ') + ' ' + amount.symbol;
}

export function format_extended_asset(amount, number_size = 18) {
    return amount_to_decimal(amount).padStart(number_size, ' ') + ' ' + amount.symbol + '@' + amount.contract;
}
