// copyright defined in LICENSE.txt

#include "ex-token.hpp"
#include "lib-database.hpp"
#include "test-common.hpp"

// todo: move
extern "C" void eosio_assert(uint32_t test, const char* msg) {
    if (!test)
        eosio_assert_message(test, msg, strlen(msg));
}

struct transfer {
    eosio::name      from     = {};
    eosio::name      to       = {};
    eosio::asset     quantity = {};
    std::string_view memo     = {nullptr, 0};
};

void process(token_transfer_request& req, const context_data& context) {
    print("    transfers\n");
    auto s = exec_query(query_action_trace_executed_range_name_receiver_account_block_trans_action{
        .max_block = get_block_num(req.max_block, context),
        .first =
            {
                .name             = "transfer"_n,
                .receipt_receiver = req.first_key.receipt_receiver,
                .account          = req.first_key.account,
                .block_index      = req.first_key.block_index,
                .transaction_id   = req.first_key.transaction_id,
                .action_index     = req.first_key.action_index,
            },
        .last =
            {
                .name             = "transfer"_n,
                .receipt_receiver = req.last_key.receipt_receiver,
                .account          = req.last_key.account,
                .block_index      = req.last_key.block_index,
                .transaction_id   = req.last_key.transaction_id,
                .action_index     = req.last_key.action_index,
            },
        .max_results = req.max_results,
    });

    print(s.size(), "\n");
    token_transfer_response response;
    for_each_query_result<action_trace>(s, [&](action_trace& at) {
        response.more = token_transfer_key{
            .receipt_receiver = at.receipt_receiver,
            .account          = at.account,
            .block_index      = at.block_index,
            .transaction_id   = at.transaction_id,
            .action_index     = at.action_index,
        };

        // todo: handle bad unpack
        auto unpacked = eosio::unpack<transfer>(at.data.pos(), at.data.remaining());

        bool is_notify = at.receipt_receiver != at.account;
        if ((req.include_notify_incoming && is_notify && at.receipt_receiver == unpacked.to) ||
            (req.include_notify_outgoing && is_notify && at.receipt_receiver == unpacked.from) || //
            (req.include_nonnotify && !is_notify)) {

            print("   ", at.block_index, " ", at.action_index, " ", at.account, " ", at.name, "\n");
            response.transfers.push_back(token_transfer{
                .key      = *response.more,
                .from     = unpacked.from,
                .to       = unpacked.to,
                .quantity = extended_asset{unpacked.quantity, at.account},
                .memo     = unpacked.memo,
            });
        }
        return true;
    });
    if (response.more)
        ++*response.more;
    set_output_data(pack(token_response{std::move(response)}));
    print("\n");
}

void process(balances_for_multiple_accounts_request& req, const context_data& context) {
    print("    balances_for_multiple_accounts\n");
    auto s = exec_query(query_contract_row_range_code_table_pk_scope{
        .max_block = get_block_num(req.max_block, context),
        .first =
            {
                .code        = req.code,
                .table       = "accounts"_n,
                .primary_key = req.sym.raw(),
                .scope       = req.first_account.value,
            },
        .last =
            {
                .code        = req.code,
                .table       = "accounts"_n,
                .primary_key = req.sym.raw(),
                .scope       = req.last_account.value,
            },
        .max_results = req.max_results,
    });

    balances_for_multiple_accounts_response response;
    for_each_contract_row<asset>(s, [&](contract_row& r, asset* a) {
        print("        ", r.block_index, " ", r.present, " ", r.code, " ", name{r.scope}, " ", r.payer);
        response.more = name{r.scope + 1};
        if (r.present && a) {
            print(" ", asset_to_string(*a));
            response.balances.push_back({.account = name{r.scope}, .amount = extended_asset{*a, req.code}});
        }
        print("\n");
        return true;
    });
    set_output_data(pack(token_response{std::move(response)}));
    print("\n");
}

void process(balances_for_multiple_tokens_request& req, const context_data& context) {
    print("    balances_for_multiple_tokens\n");
    auto s = exec_query(query_contract_row_range_scope_table_pk_code{
        .max_block = get_block_num(req.max_block, context),
        .first =
            {
                .scope       = req.account.value,
                .table       = "accounts"_n,
                .primary_key = req.first_key.sym.raw(),
                .code        = req.first_key.code,
            },
        .last =
            {
                .scope       = req.account.value,
                .table       = "accounts"_n,
                .primary_key = req.last_key.sym.raw(),
                .code        = req.last_key.code,
            },
        .max_results = req.max_results,
    });

    balances_for_multiple_tokens_response response;
    for_each_query_result<contract_row>(s, [&](contract_row& r) {
        response.more = ++bfmt_key{.sym = symbol_code{r.primary_key}, .code = r.code};
        if (!r.present || r.value.remaining() != 16)
            return true;
        asset a;
        r.value >> a;
        if (!a.is_valid() || a.symbol.code().raw() != r.primary_key)
            return true;
        print("        ", name{r.scope}, " ", r.code, " ", asset_to_string(a), "\n");
        if (!response.more->code.value)
            response.more->sym = symbol_code{response.more->sym.raw() + 1};
        if (r.present)
            response.balances.push_back({.account = name{r.scope}, .amount = extended_asset{a, r.code}});
        return true;
    });
    set_output_data(pack(token_response{std::move(response)}));
    print("\n");
}

extern "C" void startup() {
    auto request = unpack<token_request>(get_input_data());
    print("request: ", token_request::keys[request.value.index()], "\n");
    std::visit([](auto& x) { process(x, get_context_data()); }, request.value);
}
