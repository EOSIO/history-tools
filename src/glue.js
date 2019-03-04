// copyright defined in LICENSE.txt

let modules = {};
let inst;

const env = {
    abort() {
        throw new Error('called abort');
    },
    eosio_assert_message(test, msg, msg_len) {
        // todo: pass assert message through RPC API
        print_js_str("assert: ");
        print_wasm_str(inst.exports.memory.buffer, msg, msg + msg_len);
        print_js_str("\n");
        if (!test)
            throw new Error('assert failed');
    },
    get_database_status(cb_alloc_data, cb_alloc) {
        get_database_status(size => {
            // cb_alloc may resize memory, causing inst.exports.memory.buffer to change
            let ptr = inst.exports.__indirect_function_table.get(cb_alloc)(cb_alloc_data, size);
            return [inst.exports.memory.buffer, ptr];
        });
    },
    get_input_data(cb_alloc_data, cb_alloc) {
        get_input_data(size => {
            // cb_alloc may resize memory, causing inst.exports.memory.buffer to change
            let ptr = inst.exports.__indirect_function_table.get(cb_alloc)(cb_alloc_data, size);
            return [inst.exports.memory.buffer, ptr];
        });
    },
    set_output_data(begin, end) {
        set_output_data(inst.exports.memory.buffer, begin, end);
    },
    query_database(req_begin, req_end, cb_alloc_data, cb_alloc) {
        query_database(inst.exports.memory.buffer, req_begin, req_end, size => {
            // cb_alloc may resize memory, causing inst.exports.memory.buffer to change
            let ptr = inst.exports.__indirect_function_table.get(cb_alloc)(cb_alloc_data, size);
            return [inst.exports.memory.buffer, ptr];
        });
    },
    print_range(begin, end) {
        print_wasm_str(inst.exports.memory.buffer, begin, end);
    },
};

function run_query(wasm_name) {
    try {
        if (!modules[wasm_name])
            modules[wasm_name] = new WebAssembly.Module(get_wasm(wasm_name));
        inst = new WebAssembly.Instance(modules[wasm_name], { env });
        inst.exports.run_query();
    } catch (e) {
        print_js_str('Caught: ' + e + '\n');
        throw e;
    }
}

print_js_str('glue initialized\n');
