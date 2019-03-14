# Creating WASMs

The sources for a minimal WASM pair include:
* A common header (`my.hpp`)
* A server implementation (`my-server.cpp`)
* A client implementation (`my-client.cpp`)

## Header

The header declares the set of available queries and responses. Here's an example header for a WASM pair which can query token balances for a range of accounts.

```cpp
// my.hpp

#pragma once
#include <eosio/asset.hpp>
#include <eosio/block_select.hpp>

// Parameters for a query
struct get_my_tokens_request {
    eosio::block_select max_block     = {};
    eosio::name         code          = {};
    eosio::symbol_code  sym           = {};
    eosio::name         first_account = {};
    eosio::name         last_account  = {};
    uint32_t            max_results   = {};
};

// Enables JSON <> binary conversion
STRUCT_REFLECT(get_my_tokens_request) {
    STRUCT_MEMBER(get_my_tokens_request, max_block)
    STRUCT_MEMBER(get_my_tokens_request, code)
    STRUCT_MEMBER(get_my_tokens_request, sym)
    STRUCT_MEMBER(get_my_tokens_request, first_account)
    STRUCT_MEMBER(get_my_tokens_request, last_account)
    STRUCT_MEMBER(get_my_tokens_request, max_results)
}

// A single balance
struct token_balance {
    eosio::name           account = {};
    eosio::extended_asset amount  = {};
};

STRUCT_REFLECT(token_balance) {
    STRUCT_MEMBER(token_balance, account)
    STRUCT_MEMBER(token_balance, amount)
}

// Query response
struct get_my_tokens_response {
    std::vector<token_balance> balances = {};
    std::optional<eosio::name> more     = {};

    EOSLIB_SERIALIZE(get_my_tokens_response, (balances)(more))
};

STRUCT_REFLECT(get_my_tokens_response) {
    STRUCT_MEMBER(get_my_tokens_response, balances)
    STRUCT_MEMBER(get_my_tokens_response, more)
}

// The set of available queries. Each query has a name which identifies it.
using my_query_request = eosio::tagged_variant<
    eosio::serialize_tag_as_name,
    eosio::tagged_type<"get.my.toks"_n, get_my_tokens_request>>;

// The set of available responses. Each response has a name which identifies it.
// The name must match the request's name.
using my_query_response = eosio::tagged_variant<
    eosio::serialize_tag_as_name,
    eosio::tagged_type<"get.my.toks"_n, get_my_tokens_response>>;
```

## Server WASM

The server WASM uses the database to find answers to incoming requests.

```cpp
// my-server.cpp

#include "my.hpp"
#include <eosio/database.hpp>
#include <eosio/input_output.hpp>

// process this query
void process(get_my_tokens_request& req, const eosio::database_status& status) {

    // query the database for a range of contract rows,
    // ordered by (code, table, primary_key, scope)
    auto s = query_database(eosio::query_contract_row_range_code_table_pk_scope{
        // look at this point in time
        .max_block = get_block_num(req.max_block, status),

        // get records with keys >= first
        .first =
            {
                .code        = req.code,
                .table       = "accounts"_n,
                .primary_key = req.sym.raw(),
                .scope       = req.first_account.value,
            },

        // get records with keys <= last
        .last =
            {
                .code        = req.code,
                .table       = "accounts"_n,
                .primary_key = req.sym.raw(),
                .scope       = req.last_account.value,
            },

        // get this many results max. wasm-ql may return fewer results.
        .max_results = req.max_results,
    });

    get_my_tokens_response response;

    // loop through each record
    eosio::for_each_contract_row<eosio::asset>(s, [&](eosio::contract_row& r, eosio::asset* a) {
        // let requestor know how to continue the search
        response.more = eosio::name{r.scope + 1};

        // if the row is present and contains a valid asset, record the result
        if (r.present && a)
            response.balances.push_back({
                .account = eosio::name{r.scope},
                .amount = eosio::extended_asset{*a, req.code}
            });

        // continue the loop
        return true;
    });

    // send the result to the requestor
    eosio::set_output_data(pack(my_query_response{std::move(response)}));
}

// initialize this WASM
extern "C" __attribute__((eosio_wasm_entry)) void initialize() {}

// wasm-ql calls this for each incoming query
extern "C" void run_query() {
    // deserialize the request
    auto request = eosio::unpack<my_query_request>(eosio::get_input_data());

    // dispatch the request to the appropriate `process` overload
    std::visit([](auto& x) { process(x, eosio::get_database_status()); }, request.value);
}
```

## Client WASM

The client WASM converts between JSON and the binary format the server WASM understands

```cpp
// my-client.cpp

#include "my.hpp"
#include <eosio/input_output.hpp>
#include <eosio/parse_json.hpp>
#include <eosio/schema.hpp>

// initialize this WASM
extern "C" __attribute__((eosio_wasm_entry)) void initialize() {}

// produce JSON schema for request
extern "C" void describe_query_request() {
    eosio::set_output_data(eosio::make_json_schema<my_query_request>());
}

// produce JSON schema for response
extern "C" void describe_query_response() {
    eosio::set_output_data(eosio::make_json_schema<my_query_response>());
}

// convert request from JSON to binary
extern "C" void create_query_request() {
    eosio::set_output_data(pack(std::make_tuple(
        "local"_n,      // must be "local"
        "my"_n,         // name of server WASM
        eosio::parse_json<my_query_request>(eosio::get_input_data()))));
}

// convert response from binary to JSON
extern "C" void decode_query_response() {
    eosio::set_output_data(to_json(eosio::unpack<my_query_response>(eosio::get_input_data())));
}
```

## Building

Use the CDT to build the WASMs:

```sh
# Location of History Tools repository
export HT_TOOLS_DIR=~/history-tools

# Location of the CDT
export CDT_DIR=/usr/local/eosio.cdt

$CDT_DIR/bin/eosio-cpp                                                      \
    -Os                                                                     \
    -I $HT_TOOLS_DIR/libraries/eosiolib/wasmql                              \
    -I $HT_TOOLS_DIR/external/abieos/external/date/include                  \
    $HT_TOOLS_DIR/libraries/eosiolib/wasmql/eosio/temp_placeholders.cpp     \
    -fquery-server                                                          \
    --eosio-imports=$HT_TOOLS_DIR/libraries/eosiolib/wasmql/server.imports  \
    my-server.cpp                                                           \
    -o my-server.wasm

$CDT_DIR/bin/eosio-cpp                                                      \
    -Os                                                                     \
    -I $HT_TOOLS_DIR/libraries/eosiolib/wasmql                              \
    -I $HT_TOOLS_DIR/external/abieos/external/date/include                  \
    $HT_TOOLS_DIR/libraries/eosiolib/wasmql/eosio/temp_placeholders.cpp     \
    -fquery-client                                                          \
    --eosio-imports=$HT_TOOLS_DIR/libraries/eosiolib/wasmql/client.imports  \
    my-client.cpp                                                           \
    -o my-client.wasm
```

## Deploying

Place `my-server.wasm` in the directory where you run wasm-ql. Make `my-client.wasm` available to clients.

## Example use

Modify the code in [Using Client WASMs](using-client-wasms.md):

```js
// Use this for browsers:
const myClientWasm = await HistoryTools.createClientWasm({
    mod: await WebAssembly.compileStreaming(fetch('.../path/to/my-client.wasm')),
    encoder, decoder
});

// Use this for nodejs
const myClientWasm = await HistoryTools.createClientWasm({
    mod: new WebAssembly.Module(fs.readFileSync('.../path/to/my-client.wasm')),
    encoder, decoder
});

// Use this for either:
const request = myClientWasm.createQueryRequest(JSON.stringify(
    ['get.my.toks', {
        max_block: ['head', 0],
        code: 'eosio.token',
        sym: 'EOS',
        first_account: 'eosio',
        last_account: 'eosio.zzzzzz',
        max_results: 10,
    }]
));

const queryReply = await fetch('http(s)://server.name:port/wasmql/v1/query', {
    method: 'POST',
    body: HistoryTools.combineRequests([
        request,
    ]),
});
if (queryReply.status !== 200)
    throw new Error(queryReply.status + ': ' + queryReply.statusText + ': ' + await queryReply.text());

const responses = HistoryTools.splitResponses(new Uint8Array(await queryReply.arrayBuffer()));

prettyPrint('The tokens', myClientWasm.decodeQueryResponse(responses[0]));
```
