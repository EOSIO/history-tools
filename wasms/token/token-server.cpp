// copyright defined in LICENSE.txt

#include "token.hpp"

#include <eosio/database.hpp>
#include <eosio/input-output.hpp>

struct transfer {
    eosio::name      from     = {};
    eosio::name      to       = {};
    eosio::asset     quantity = {};
    std::string_view memo     = {nullptr, 0};
};

void process(token_transfer_request& req, const eosio::database_status& status) {
    eosio::print("    transfers\n");
    using query_type = eosio::query_action_trace_executed_range_name_receiver_account_block_trans_action;
    auto s           = query_database(query_type{
        .max_block = get_block_num(req.max_block, status),
        .first =
            {
                .name             = "transfer"_n,
                .receipt_receiver = req.first_key.receipt_receiver,
                .account          = req.first_key.account,
                .block_index      = get_block_num(req.first_key.block, status),
                .transaction_id   = req.first_key.transaction_id,
                .action_index     = req.first_key.action_index,
            },
        .last =
            {
                .name             = "transfer"_n,
                .receipt_receiver = req.last_key.receipt_receiver,
                .account          = req.last_key.account,
                .block_index      = get_block_num(req.last_key.block, status),
                .transaction_id   = req.last_key.transaction_id,
                .action_index     = req.last_key.action_index,
            },
        .max_results = req.max_results,
    });

    eosio::print(s.size(), "\n");
    token_transfer_response        response;
    std::optional<query_type::key> last_key;
    eosio::for_each_query_result<eosio::action_trace>(s, [&](eosio::action_trace& at) {
        last_key = query_type::key::from_data(at);

        // todo: handle bad unpack
        auto unpacked = eosio::unpack<transfer>(at.data.pos(), at.data.remaining());

        bool is_notify = at.receipt_receiver != at.account;
        if ((req.include_notify_incoming && is_notify && at.receipt_receiver == unpacked.to) ||
            (req.include_notify_outgoing && is_notify && at.receipt_receiver == unpacked.from) || //
            (req.include_nonnotify && !is_notify)) {

            eosio::print("   ", at.block_index, " ", at.action_index, " ", at.account, " ", at.name, "\n");
            response.transfers.push_back(token_transfer{
                .key =
                    {
                        .receipt_receiver = at.receipt_receiver,
                        .account          = at.account,
                        .block            = eosio::make_absolute_block(at.block_index),
                        .transaction_id   = at.transaction_id,
                        .action_index     = at.action_index,
                    },
                .from     = unpacked.from,
                .to       = unpacked.to,
                .quantity = eosio::extended_asset{unpacked.quantity, at.account},
                .memo     = unpacked.memo,
            });
        }
        return true;
    });
    if (last_key) {
        increment_key(*last_key);
        response.more = token_transfer_key{
            .receipt_receiver = last_key->receipt_receiver,
            .account          = last_key->account,
            .block            = eosio::make_absolute_block(last_key->block_index),
            .transaction_id   = last_key->transaction_id,
            .action_index     = last_key->action_index,
        };
    }
    eosio::set_output_data(pack(token_query_response{std::move(response)}));
    eosio::print("\n");
}

void process(balances_for_multiple_accounts_request& req, const eosio::database_status& status) {
    eosio::print("    balances_for_multiple_accounts\n");
    auto s = query_database(eosio::query_contract_row_range_code_table_pk_scope{
        .max_block = get_block_num(req.max_block, status),
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
    eosio::for_each_contract_row<eosio::asset>(s, [&](eosio::contract_row& r, eosio::asset* a) {
        eosio::print("        ", r.block_index, " ", r.present, " ", r.code, " ", eosio::name{r.scope}, " ", r.payer);
        response.more = eosio::name{r.scope + 1};
        if (r.present && a) {
            eosio::print(" ", asset_to_string(*a));
            response.balances.push_back({.account = eosio::name{r.scope}, .amount = eosio::extended_asset{*a, req.code}});
        }
        eosio::print("\n");
        return true;
    });
    eosio::set_output_data(pack(token_query_response{std::move(response)}));
    eosio::print("\n");
}

void process(balances_for_multiple_tokens_request& req, const eosio::database_status& status) {
    eosio::print("    balances_for_multiple_tokens\n");
    auto s = query_database(eosio::query_contract_row_range_scope_table_pk_code{
        .max_block = get_block_num(req.max_block, status),
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
    eosio::for_each_query_result<eosio::contract_row>(s, [&](eosio::contract_row& r) {
        response.more = ++bfmt_key{.sym = eosio::symbol_code{r.primary_key}, .code = r.code};
        if (!r.present || r.value.remaining() != 16)
            return true;
        eosio::asset a;
        r.value >> a;
        if (!a.is_valid() || a.symbol.code().raw() != r.primary_key)
            return true;
        eosio::print("        ", eosio::name{r.scope}, " ", r.code, " ", asset_to_string(a), "\n");
        if (!response.more->code.value)
            response.more->sym = eosio::symbol_code{response.more->sym.raw() + 1};
        if (r.present)
            response.balances.push_back({.account = eosio::name{r.scope}, .amount = eosio::extended_asset{a, r.code}});
        return true;
    });
    eosio::set_output_data(pack(token_query_response{std::move(response)}));
    eosio::print("\n");
}

// todo: remove "uint64_t, uint64_t, uint64_t" after CDT changes
extern "C" void run_query(uint64_t, uint64_t, uint64_t) {
    auto request = eosio::unpack<token_query_request>(eosio::get_input_data());
    eosio::print("request: ", token_query_request::keys[request.value.index()], "\n");
    std::visit([](auto& x) { process(x, eosio::get_database_status()); }, request.value);
}
