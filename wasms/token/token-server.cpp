// copyright defined in LICENSE.txt

#include "token.hpp"

#include <eosio/database.hpp>
#include <eosio/input_output.hpp>

struct transfer {
    eosio::name                            from     = {};
    eosio::name                            to       = {};
    eosio::asset                           quantity = {};
    eosio::shared_memory<std::string_view> memo     = {};
};

void process(token_transfer_request& req, const eosio::database_status& status) {
    using query_type = eosio::query_action_trace_range_name_receiver_account_block_trans_action;
    auto s           = query_database(query_type{
        .snapshot_block = get_block_num(req.snapshot_block, status),
        .first =
            {
                .name           = "transfer"_n,
                .receiver       = req.first_key.receiver,
                .account        = req.first_key.account,
                .block_num      = get_block_num(req.first_key.block, status),
                .transaction_id = req.first_key.transaction_id,
                .action_ordinal = req.first_key.action_ordinal,
            },
        .last =
            {
                .name           = "transfer"_n,
                .receiver       = req.last_key.receiver,
                .account        = req.last_key.account,
                .block_num      = get_block_num(req.last_key.block, status),
                .transaction_id = req.last_key.transaction_id,
                .action_ordinal = req.last_key.action_ordinal,
            },
        .max_results = req.max_results,
    });

    token_transfer_response        response;
    std::optional<query_type::key> last_key;
    eosio::for_each_query_result<eosio::action_trace>(s, [&](eosio::action_trace& at) {
        last_key = query_type::key::from_data(at);
        if (at.transaction_status != eosio::transaction_status::executed)
            return true;

        // todo: handle bad unpack
        auto unpacked = eosio::unpack<transfer>(at.action.data->pos(), at.action.data->remaining());

        bool is_notify = at.receiver != at.action.account;
        if ((req.include_notify_incoming && is_notify && at.receiver == unpacked.to) ||
            (req.include_notify_outgoing && is_notify && at.receiver == unpacked.from) || //
            (req.include_nonnotify && !is_notify)) {

            response.transfers.push_back(token_transfer{
                .key =
                    {
                        .receiver       = at.receiver,
                        .account        = at.action.account,
                        .block          = eosio::make_absolute_block(at.block_num),
                        .transaction_id = at.transaction_id,
                        .action_ordinal = at.action_ordinal,
                    },
                .from     = unpacked.from,
                .to       = unpacked.to,
                .quantity = eosio::extended_asset{unpacked.quantity, at.action.account},
                .memo     = unpacked.memo,
            });
        }
        return true;
    });
    if (last_key) {
        increment_key(*last_key);
        response.more = token_transfer_key{
            .receiver       = last_key->receiver,
            .account        = last_key->account,
            .block          = eosio::make_absolute_block(last_key->block_num),
            .transaction_id = last_key->transaction_id,
            .action_ordinal = last_key->action_ordinal,
        };
    }
    eosio::set_output_data(pack(token_query_response{std::move(response)}));
}

void process(balances_for_multiple_accounts_request& req, const eosio::database_status& status) {
    auto s = query_database(eosio::query_contract_row_range_code_table_pk_scope{
        .snapshot_block = get_block_num(req.snapshot_block, status),
        .first =
            {
                .code        = req.code,
                .table       = "accounts"_n,
                .primary_key = req.sym.raw(),
                .scope       = req.first_account,
            },
        .last =
            {
                .code        = req.code,
                .table       = "accounts"_n,
                .primary_key = req.sym.raw(),
                .scope       = req.last_account,
            },
        .max_results = req.max_results,
    });

    balances_for_multiple_accounts_response response;
    eosio::for_each_contract_row<eosio::asset>(s, [&](eosio::contract_row& r, eosio::asset* a) {
        response.more = eosio::name{r.scope.value + 1};
        if (r.present && a)
            response.balances.push_back({.account = eosio::name{r.scope}, .amount = eosio::extended_asset{*a, req.code}});
        return true;
    });
    eosio::set_output_data(pack(token_query_response{std::move(response)}));
}

void process(balances_for_multiple_tokens_request& req, const eosio::database_status& status) {
    auto s = query_database(eosio::query_contract_row_range_scope_table_pk_code{
        .snapshot_block = get_block_num(req.snapshot_block, status),
        .first =
            {
                .scope       = req.account,
                .table       = "accounts"_n,
                .primary_key = req.first_key.sym.raw(),
                .code        = req.first_key.code,
            },
        .last =
            {
                .scope       = req.account,
                .table       = "accounts"_n,
                .primary_key = req.last_key.sym.raw(),
                .code        = req.last_key.code,
            },
        .max_results = req.max_results,
    });

    balances_for_multiple_tokens_response response;
    eosio::for_each_query_result<eosio::contract_row>(s, [&](eosio::contract_row& r) {
        response.more = ++bfmt_key{.sym = eosio::symbol_code{r.primary_key}, .code = r.code};
        if (!r.present || r.value->remaining() != 16)
            return true;
        eosio::asset a;
        *r.value >> a;
        if (!a.is_valid() || a.symbol.code().raw() != r.primary_key)
            return true;
        if (!response.more->code.value)
            response.more->sym = eosio::symbol_code{response.more->sym.raw() + 1};
        if (r.present)
            response.balances.push_back({.account = eosio::name{r.scope}, .amount = eosio::extended_asset{a, r.code}});
        return true;
    });
    eosio::set_output_data(pack(token_query_response{std::move(response)}));
}

extern "C" __attribute__((eosio_wasm_entry)) void initialize() {}

extern "C" void run_query() {
    auto request = eosio::unpack<token_query_request>(eosio::get_input_data());
    std::visit([](auto& x) { process(x, eosio::get_database_status()); }, request.value);
}
