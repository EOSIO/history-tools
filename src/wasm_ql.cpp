// copyright defined in LICENSE.txt

#include "wasm_ql.hpp"
#include "chaindb_callbacks.hpp"
#include "memory_callbacks.hpp"
#include "state_history_rocksdb.hpp"
#include "unimplemented_callbacks.hpp"

namespace eosio {
using state_history::rdb::kv_environment;
}
#include "../wasms/state_history_kv_tables.hpp" // todo: move

#include <fc/log/logger.hpp>
#include <fc/scoped_exit.hpp>

using namespace abieos::literals;
using namespace std::literals;

namespace wasm_ql {

// todo: replace
struct hex_bytes {
    std::vector<char> data = {};
};

template <typename S>
eosio::result<void> from_json(hex_bytes& obj, S& stream) {
    return from_json_hex(obj.data, stream);
}

// todo: remove
struct dummy_type {};

EOSIO_REFLECT(dummy_type)

// todo: replace
struct result_action_trace {
    abieos::varuint32          action_ordinal         = {};
    abieos::varuint32          creator_action_ordinal = {};
    std::optional<dummy_type>  receipt                = {};
    abieos::name               receiver               = {};
    state_history::action      act                    = {};
    bool                       context_free           = {};
    int64_t                    elapsed                = {};
    std::string                console                = {};
    std::vector<dummy_type>    account_ram_deltas     = {};
    std::optional<std::string> except                 = {};
    std::optional<uint64_t>    error_code             = {};
};

EOSIO_REFLECT(
    result_action_trace, action_ordinal, creator_action_ordinal, receipt, receiver, act, context_free, elapsed, console, account_ram_deltas,
    except, error_code)

// todo: replace
struct result_transaction_trace {
    abieos::checksum256               id                = {};
    state_history::transaction_status status            = {};
    uint32_t                          cpu_usage_us      = {};
    abieos::varuint32                 net_usage_words   = {};
    int64_t                           elapsed           = {};
    uint64_t                          net_usage         = {};
    bool                              scheduled         = {};
    std::vector<result_action_trace>  action_traces     = {};
    std::optional<dummy_type>         account_ram_delta = {};
    std::optional<std::string>        except            = {};
    std::optional<uint64_t>           error_code        = {};
    std::vector<dummy_type>           failed_dtrx_trace = {};
};

EOSIO_REFLECT(
    result_transaction_trace, id, status, cpu_usage_us, net_usage_words, elapsed, net_usage, scheduled, action_traces, account_ram_delta,
    except, error_code, failed_dtrx_trace)

struct callbacks;
using backend_t = eosio::vm::backend<callbacks, eosio::vm::jit>;
using rhf_t     = eosio::vm::registered_host_functions<callbacks>;

// todo: remove basic_callbacks
struct callbacks : history_tools::action_callbacks<callbacks>,
                   history_tools::basic_callbacks<callbacks>,
                   history_tools::chaindb_callbacks<callbacks>,
                   history_tools::memory_callbacks<callbacks>,
                   history_tools::unimplemented_callbacks<callbacks> {
    wasm_ql::thread_state&             thread_state;
    history_tools::chaindb_state&      chaindb_state;
    state_history::rdb::db_view_state& db_view_state;

    callbacks(
        wasm_ql::thread_state& thread_state, history_tools::chaindb_state& chaindb_state, state_history::rdb::db_view_state& db_view_state)
        : thread_state{thread_state}
        , chaindb_state{chaindb_state}
        , db_view_state{db_view_state} {}

    auto& get_state() { return thread_state; }
    auto& get_chaindb_state() { return chaindb_state; }
    auto& get_db_view_state() { return db_view_state; }
};

void register_callbacks() {
    history_tools::action_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
    history_tools::basic_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
    history_tools::chaindb_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
    history_tools::memory_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
    history_tools::unimplemented_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
}

// todo: timeout
// todo: limit WASM memory size
static void run_query(wasm_ql::thread_state& thread_state, state_history::action& action) {
    if (thread_state.shared->contract_dir.empty())
        throw std::runtime_error("contract_dir is empty"); // todo
    auto                     code = backend_t::read_wasm(thread_state.shared->contract_dir + "/" + (std::string)action.account + ".wasm");
    backend_t                backend(code);
    rocksdb::ManagedSnapshot snapshot{thread_state.shared->db->rdb.get()};
    chain_kv::write_session  write_session{*thread_state.shared->db, snapshot.snapshot()};
    state_history::rdb::db_view_state db_view_state{abieos::name{"state"}, *thread_state.shared->db, write_session};
    history_tools::chaindb_state      chaindb_state;
    callbacks                         cb{thread_state, chaindb_state, db_view_state};
    backend.set_wasm_allocator(&thread_state.wa);
    thread_state.receiver    = action.account;
    thread_state.action_data = action.data;
    thread_state.action_return_value.clear();

    rhf_t::resolve(backend.get_module());
    backend.initialize(&cb);
    backend(&cb, "env", "apply", action.account.value, action.account.value, action.name.value);
}

const std::vector<char>& query_get_info(wasm_ql::thread_state& thread_state) {
    rocksdb::ManagedSnapshot          snapshot{thread_state.shared->db->rdb.get()};
    chain_kv::write_session           write_session{*thread_state.shared->db, snapshot.snapshot()};
    state_history::rdb::db_view_state db_view_state{abieos::name{"state"}, *thread_state.shared->db, write_session};

    std::string result = "{\"server-type\":\"wasm-ql\"";

    {
        state_history::global_property_kv table{{db_view_state}};
        bool                              found = false;
        if (table.begin() != table.end()) {
            auto record = table.begin().get();
            if (auto* obj = std::get_if<state_history::global_property_v1>(&record)) {
                found = true;
                result += ",\"chain_id\":\"" + (std::string)obj->chain_id + "\"";
            }
        }
        if (!found)
            throw std::runtime_error("No global_property_v1 records found; is filler running?");
    }

    {
        state_history::fill_status_kv table{{db_view_state}};
        if (table.begin() != table.end()) {
            std::visit(
                [&](auto& obj) {
                    result += ",\"head_block_num\":\"" + std::to_string(obj.head) + "\"";
                    result += ",\"head_block_id\":\"" + (std::string)obj.head_id + "\"";
                    result += ",\"last_irreversible_block_num\":\"" + std::to_string(obj.irreversible) + "\"";
                    result += ",\"last_irreversible_block_id\":\"" + (std::string)obj.irreversible_id + "\"";
                },
                table.begin().get());
        } else
            throw std::runtime_error("No fill_status records found; is filler running?");
    }

    result += "}";

    thread_state.action_return_value.assign(result.data(), result.data() + result.size());
    return thread_state.action_return_value;
}

struct get_block_params {
    uint32_t block_num_or_id = {};
};

EOSIO_REFLECT(get_block_params, block_num_or_id)

const std::vector<char>& query_get_block(wasm_ql::thread_state& thread_state, std::string_view body) {
    get_block_params         params;
    std::string              s{body.begin(), body.end()};
    eosio::json_token_stream stream{s.data()};
    auto                     r = from_json(params, stream);
    if (!r)
        throw std::runtime_error("An error occurred deserializing get_block_params: " + r.error().message());

    rocksdb::ManagedSnapshot          snapshot{thread_state.shared->db->rdb.get()};
    chain_kv::write_session           write_session{*thread_state.shared->db, snapshot.snapshot()};
    state_history::rdb::db_view_state db_view_state{abieos::name{"state"}, *thread_state.shared->db, write_session};
    state_history::block_info_kv      table{{db_view_state}};

    // !!!! todo: use index
    // todo: look up by id? rename block_num_or_id?
    for (auto it = table.begin(); it != table.end(); ++it) {
        auto obj = std::get<0>(it.get());
        if (obj.num == params.block_num_or_id) {
            uint32_t ref_block_prefix;
            memcpy(&ref_block_prefix, obj.id.value.begin() + 8, sizeof(ref_block_prefix));

            std::string sig, error;
            if (!abieos::signature_to_string(sig, error, obj.producer_signature))
                throw std::runtime_error("producer_signature: " + error);

            std::string result = "{";
            result += "\"block_num\":" + std::to_string(obj.num);
            result += ",\"id\":\"" + (std::string)obj.id + "\"";
            result += ",\"timestamp\":\"" + (std::string)obj.timestamp + "\"";
            result += ",\"producer\":\"" + (std::string)obj.producer + "\"";
            result += ",\"confirmed\":" + std::to_string(obj.confirmed);
            result += ",\"previous\":\"" + (std::string)obj.previous + "\"";
            result += ",\"transaction_mroot\":\"" + (std::string)obj.transaction_mroot + "\"";
            result += ",\"action_mroot\":\"" + (std::string)obj.action_mroot + "\"";
            result += ",\"schedule_version\":" + std::to_string(obj.schedule_version);
            result += ",\"producer_signature\":\"" + sig + "\"";
            result += ",\"ref_block_prefix\":" + std::to_string(ref_block_prefix);
            result += "}";

            thread_state.action_return_value.assign(result.data(), result.data() + result.size());
            return thread_state.action_return_value;
        }
    }

    throw std::runtime_error("block " + std::to_string(params.block_num_or_id) + " not found");
} // query_get_block

struct send_transaction_params {
    std::vector<abieos::signature> signatures               = {};
    uint8_t                        compression              = {};
    hex_bytes                      packed_context_free_data = {};
    hex_bytes                      packed_trx               = {};
};

EOSIO_REFLECT(send_transaction_params, signatures, compression, packed_context_free_data, packed_trx)

struct send_transaction_results {
    abieos::checksum256      transaction_id; // todo: redundant with processed.id
    result_transaction_trace processed;
};

EOSIO_REFLECT(send_transaction_results, transaction_id, processed)

const std::vector<char>& query_send_transaction(wasm_ql::thread_state& thread_state, std::string_view body) {
    send_transaction_params trx;
    {
        std::string              s{body.begin(), body.end()};
        eosio::json_token_stream stream{s.data()};
        if (auto r = from_json(trx, stream); !r)
            throw std::runtime_error("An error occurred deserializing send_transaction_params: "s + r.error().message());
    }
    eosio::input_stream s{trx.packed_trx.data};
    auto                r = eosio::from_bin<state_history::transaction>(s);
    if (!r)
        throw std::runtime_error("An error occurred deserializing packed_trx: "s + r.error().message());
    if (s.end != s.pos)
        throw std::runtime_error("Extra data in packed_trx");
    state_history::transaction& unpacked = r.value();

    if (!trx.signatures.empty())
        throw std::runtime_error("Signatures must be empty"); // todo
    if (trx.compression)
        throw std::runtime_error("Compression must be 0"); // todo
    if (!trx.packed_context_free_data.data.empty())
        throw std::runtime_error("packed_context_free_data must be empty");
    // todo: verify query transaction extension is present, but no others
    // todo: redirect if transaction extension not present?
    if (!unpacked.transaction_extensions.empty())
        throw std::runtime_error("transaction_extensions must be empty");
    // todo: check expiration, ref_block_num, ref_block_prefix
    if (unpacked.delay_sec.value)
        throw std::runtime_error("delay_sec must be 0"); // queries can't be deferred
    if (!unpacked.context_free_actions.empty())
        throw std::runtime_error("context_free_actions must be empty"); // todo: is there a case where CFA makes sense?

    // todo: fill transaction_id
    send_transaction_results results;
    auto&                    tt = results.processed;
    tt.action_traces.reserve(unpacked.actions.size());

    // todo: timeout
    for (auto& action : unpacked.actions) {
        tt.action_traces.emplace_back();
        auto& at                = tt.action_traces.back();
        at.action_ordinal.value = tt.action_traces.size(); // starts at 1
        at.receiver             = action.account;
        at.act                  = action;

        // todo: store exceptions
        if (!action.authorization.empty())
            throw std::runtime_error("authorization must be empty"); // todo
        run_query(thread_state, action);
    }

    // todo: avoid the extra copy
    auto json = eosio::check(eosio::convert_to_json(results));
    thread_state.action_return_value.assign(json.value().begin(), json.value().end());
    return thread_state.action_return_value;
} // query_send_transaction

} // namespace wasm_ql
