// copyright defined in LICENSE.txt

#include "wasm_ql.hpp"
#include "chaindb_callbacks.hpp"
#include "compiler_builtins_callbacks.hpp"
#include "console_callbacks.hpp"
#include "memory_callbacks.hpp"
#include "state_history_rocksdb.hpp"
#include "unimplemented_callbacks.hpp"

#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>

#include <eosio/abi.hpp>

namespace eosio {
using state_history::rdb::kv_environment;
}
#include "../wasms/state_history_kv_tables.hpp" // todo: move

#include <fc/log/logger.hpp>
#include <fc/scoped_exit.hpp>

using namespace std::literals;

using boost::multi_index::indexed_by;
using boost::multi_index::member;
using boost::multi_index::multi_index_container;
using boost::multi_index::ordered_non_unique;
using boost::multi_index::sequenced;
using boost::multi_index::tag;

namespace eosio {

// todo: move to abieos
template <typename T, typename S>
result<void> to_json(const might_not_exist<T>& val, S& stream) {
   return to_json(val.value, stream);
}

// todo: abieos support for pair. Used by extensions_type.
template <typename S>
result<void> to_json(const std::pair<uint16_t, std::vector<char>>&, S& stream) {
   return stream_error::bad_variant_index;
}

}; // namespace eosio

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
   abieos::bytes              return_value           = {};
};

EOSIO_REFLECT(result_action_trace, action_ordinal, creator_action_ordinal, receipt, receiver, act, context_free,
              elapsed, console, account_ram_deltas, except, error_code, return_value)

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

EOSIO_REFLECT(result_transaction_trace, id, status, cpu_usage_us, net_usage_words, elapsed, net_usage, scheduled,
              action_traces, account_ram_delta, except, error_code, failed_dtrx_trace)

struct callbacks;
using backend_t = eosio::vm::backend<callbacks, eosio::vm::jit>;
using rhf_t     = eosio::vm::registered_host_functions<callbacks>;

// todo: remove basic_callbacks
struct callbacks : history_tools::action_callbacks<callbacks>,
                   history_tools::basic_callbacks<callbacks>,
                   history_tools::chaindb_callbacks<callbacks>,
                   history_tools::console_callbacks<callbacks>,
                   history_tools::compiler_builtins_callbacks<callbacks>,
                   history_tools::memory_callbacks<callbacks>,
                   history_tools::unimplemented_callbacks<callbacks> {
   wasm_ql::thread_state&             thread_state;
   history_tools::chaindb_state&      chaindb_state;
   state_history::rdb::db_view_state& db_view_state;

   callbacks(wasm_ql::thread_state& thread_state, history_tools::chaindb_state& chaindb_state,
             state_history::rdb::db_view_state& db_view_state)
       : thread_state{ thread_state }, chaindb_state{ chaindb_state }, db_view_state{ db_view_state } {}

   auto& get_state() { return thread_state; }
   auto& get_chaindb_state() { return chaindb_state; }
   auto& get_db_view_state() { return db_view_state; }
};

void register_callbacks() {
   history_tools::action_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
   history_tools::basic_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
   history_tools::chaindb_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
   history_tools::console_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
   history_tools::compiler_builtins_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
   history_tools::memory_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
   history_tools::unimplemented_callbacks<callbacks>::register_callbacks<rhf_t, eosio::vm::wasm_allocator>();
}

struct backend_entry {
   eosio::name                name; // only for wasms loaded from disk
   eosio::checksum256         hash; // only for wasms loaded from chain
   std::unique_ptr<backend_t> backend;
};

struct by_age;
struct by_name;
struct by_hash;

using backend_container = multi_index_container<
      backend_entry,
      indexed_by<sequenced<tag<by_age>>, //
                 ordered_non_unique<tag<by_name>, member<backend_entry, eosio::name, &backend_entry::name>>,
                 ordered_non_unique<tag<by_hash>, member<backend_entry, eosio::checksum256, &backend_entry::hash>>>>;

class backend_cache {
 private:
   std::mutex                   mutex;
   const wasm_ql::shared_state& shared_state;
   backend_container            backends;

 public:
   backend_cache(const wasm_ql::shared_state& shared_state) : shared_state{ shared_state } {}

   void add(backend_entry&& entry) {
      std::lock_guard<std::mutex> lock{ mutex };
      auto&                       ind = backends.get<by_age>();
      ind.push_back(std::move(entry));
      while (ind.size() > shared_state.wasm_cache_size) ind.pop_front();
   }

   std::optional<backend_entry> get(eosio::name name) {
      std::optional<backend_entry> result;
      std::lock_guard<std::mutex>  lock{ mutex };
      auto&                        ind = backends.get<by_name>();
      auto                         it  = ind.find(name);
      if (it == ind.end())
         return result;
      ind.modify(it, [&](auto& x) { result = std::move(x); });
      ind.erase(it);
      return result;
   }

   std::optional<backend_entry> get(const eosio::checksum256& hash) {
      std::optional<backend_entry> result;
      std::lock_guard<std::mutex>  lock{ mutex };
      auto&                        ind = backends.get<by_hash>();
      auto                         it  = ind.find(hash);
      if (it == ind.end())
         return result;
      ind.modify(it, [&](auto& x) { result = std::move(x); });
      ind.erase(it);
      return result;
   }
};

shared_state::shared_state(std::shared_ptr<chain_kv::database> db)
    : backend_cache(std::make_unique<wasm_ql::backend_cache>(*this)), db(std::move(db)) {}

shared_state::~shared_state() {}

template <typename T, typename K>
std::optional<std::pair<std::shared_ptr<const chain_kv::bytes>, T>> get_state_row(chain_kv::view& view, const K& key) {
   std::optional<std::pair<std::shared_ptr<const chain_kv::bytes>, T>> result;
   result.emplace();
   result->first =
         view.get(eosio::name{ "state" }.value, chain_kv::to_slice(eosio::check(eosio::convert_to_key(key)).value()));
   if (!result->first) {
      result.reset();
      return result;
   }
   eosio::input_stream account_metadata_stream{ *result->first };
   if (auto r = from_bin(result->second, account_metadata_stream); !r)
      throw std::runtime_error("An error occurred deserializing state: " + r.error().message());
   return result;
}

std::optional<std::vector<uint8_t>> read_code(wasm_ql::thread_state& thread_state, eosio::name account) {
   std::optional<std::vector<uint8_t>> code;
   if (!thread_state.shared->contract_dir.empty()) {
      auto          filename = thread_state.shared->contract_dir + "/" + (std::string)account + ".wasm";
      std::ifstream wasm_file(filename, std::ios::binary);
      if (wasm_file.is_open()) {
         ilog("compiling %{f}", ("f", filename));
         wasm_file.seekg(0, std::ios::end);
         int len = wasm_file.tellg();
         if (len < 0)
            throw std::runtime_error("wasm file length is -1");
         code.emplace(len);
         wasm_file.seekg(0, std::ios::beg);
         wasm_file.read((char*)code->data(), code->size());
         wasm_file.close();
      }
   }
   return code;
}

std::optional<eosio::checksum256> get_contract_hash(state_history::rdb::db_view_state& db_view_state,
                                                    eosio::name                        account) {
   std::optional<eosio::checksum256> result;
   auto                              meta = get_state_row<state_history::account_metadata>(
         db_view_state.kv_state.view,
         std::make_tuple(eosio::name{ "account.meta" }, eosio::name{ "primary" }, account));
   if (!meta)
      return result;
   auto& meta0 = std::get<state_history::account_metadata_v0>(meta->second);
   if (!meta0.code->vm_type && !meta0.code->vm_version)
      result = meta0.code->code_hash;
   return result;
}

std::optional<std::vector<uint8_t>> read_contract(state_history::rdb::db_view_state& db_view_state,
                                                  const eosio::checksum256& hash, eosio::name account) {
   std::optional<std::vector<uint8_t>> result;
   auto                                code_row = get_state_row<state_history::code>(
         db_view_state.kv_state.view,
         std::make_tuple(eosio::name{ "code" }, eosio::name{ "primary" }, uint8_t(0), uint8_t(0), hash));
   if (!code_row)
      return result;
   auto& code0 = std::get<state_history::code_v0>(code_row->second);

   // todo: avoid copy
   result.emplace(code0.code.pos, code0.code.end);
   ilog("compiling ${h}: ${a}", ("h", eosio::check(eosio::convert_to_json(hash)).value())("a", (std::string)account));
   return result;
}

// todo: timeout
// todo: limit WASM memory size
// todo: cache compiled wasms
static void run_query(wasm_ql::thread_state& thread_state, state_history::action& action, result_action_trace& atrace) {
   rocksdb::ManagedSnapshot          snapshot{ thread_state.shared->db->rdb.get() };
   chain_kv::write_session           write_session{ *thread_state.shared->db, snapshot.snapshot() };
   state_history::rdb::db_view_state db_view_state{ eosio::name{ "state" }, *thread_state.shared->db, write_session };

   std::optional<backend_entry>        entry = thread_state.shared->backend_cache->get(action.account);
   std::optional<std::vector<uint8_t>> code;
   if (!entry)
      code = read_code(thread_state, action.account);
   std::optional<eosio::checksum256> hash;
   if (!entry && !code) {
      hash = get_contract_hash(db_view_state, action.account);
      if (hash) {
         entry = thread_state.shared->backend_cache->get(*hash);
         if (!entry)
            code = read_contract(db_view_state, *hash, action.account);
      }
   }

   // todo: fail? silent success like normal transactions?
   if (!entry && !code)
      throw std::runtime_error("account " + (std::string)action.account + " has no code");

   if (!entry) {
      entry.emplace();
      if (hash)
         entry->hash = *hash;
      else
         entry->name = action.account;
      entry->backend = std::make_unique<backend_t>(*code);
      rhf_t::resolve(entry->backend->get_module());
   }

   thread_state.max_console_size = thread_state.shared->max_console_size;
   thread_state.receiver         = action.account;
   thread_state.action_data      = action.data;
   thread_state.action_return_value.clear();

   history_tools::chaindb_state chaindb_state;
   callbacks                    cb{ thread_state, chaindb_state, db_view_state };
   entry->backend->set_wasm_allocator(&thread_state.wa);
   entry->backend->initialize(&cb);

   (*entry->backend)(&cb, "env", "apply", action.account.value, action.account.value, action.name.value);
   atrace.console           = std::move(thread_state.console);
   atrace.return_value.data = std::move(thread_state.action_return_value);

   thread_state.shared->backend_cache->add(std::move(*entry));
}

const std::vector<char>& query_get_info(wasm_ql::thread_state& thread_state) {
   rocksdb::ManagedSnapshot          snapshot{ thread_state.shared->db->rdb.get() };
   chain_kv::write_session           write_session{ *thread_state.shared->db, snapshot.snapshot() };
   state_history::rdb::db_view_state db_view_state{ abieos::name{ "state" }, *thread_state.shared->db, write_session };

   std::string result = "{\"server-type\":\"wasm-ql\"";

   {
      state_history::global_property_kv table{ { db_view_state } };
      bool                              found = false;
      if (table.begin() != table.end()) {
         auto record = table.begin().get();
         if (auto* obj = std::get_if<state_history::global_property_v1>(&record)) {
            found = true;
            result += ",\"chain_id\":" + eosio::check(eosio::convert_to_json(obj->chain_id)).value();
         }
      }
      if (!found)
         throw std::runtime_error("No global_property_v1 records found; is filler running?");
   }

   {
      state_history::fill_status_kv table{ { db_view_state } };
      if (table.begin() != table.end()) {
         std::visit(
               [&](auto& obj) {
                  result += ",\"head_block_num\":\"" + std::to_string(obj.head) + "\"";
                  result += ",\"head_block_id\":" + eosio::check(eosio::convert_to_json(obj.head_id)).value();
                  result += ",\"last_irreversible_block_num\":\"" + std::to_string(obj.irreversible) + "\"";
                  result += ",\"last_irreversible_block_id\":" +
                            eosio::check(eosio::convert_to_json(obj.irreversible_id)).value();
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
   std::string              s{ body.begin(), body.end() };
   eosio::json_token_stream stream{ s.data() };
   auto                     r = from_json(params, stream);
   if (!r)
      throw std::runtime_error("An error occurred deserializing get_block_params: " + r.error().message());

   rocksdb::ManagedSnapshot          snapshot{ thread_state.shared->db->rdb.get() };
   chain_kv::write_session           write_session{ *thread_state.shared->db, snapshot.snapshot() };
   state_history::rdb::db_view_state db_view_state{ abieos::name{ "state" }, *thread_state.shared->db, write_session };
   state_history::block_info_kv      table{ { db_view_state } };

   // !!!! todo: use index
   // todo: look up by id? rename block_num_or_id?
   for (auto it = table.begin(); it != table.end(); ++it) {
      auto obj = std::get<0>(it.get());
      if (obj.num == params.block_num_or_id) {
         uint32_t ref_block_prefix;
         memcpy(&ref_block_prefix, obj.id.value.begin() + 8, sizeof(ref_block_prefix));

         std::string result = "{";
         result += "\"block_num\":" + eosio::check(eosio::convert_to_json(obj.num)).value();
         result += ",\"id\":" + eosio::check(eosio::convert_to_json(obj.id)).value();
         result += ",\"timestamp\":" + eosio::check(eosio::convert_to_json(obj.timestamp)).value();
         result += ",\"producer\":" + eosio::check(eosio::convert_to_json(obj.producer)).value();
         result += ",\"confirmed\":" + eosio::check(eosio::convert_to_json(obj.confirmed)).value();
         result += ",\"previous\":" + eosio::check(eosio::convert_to_json(obj.previous)).value();
         result += ",\"transaction_mroot\":" + eosio::check(eosio::convert_to_json(obj.transaction_mroot)).value();
         result += ",\"action_mroot\":" + eosio::check(eosio::convert_to_json(obj.action_mroot)).value();
         result += ",\"schedule_version\":" + eosio::check(eosio::convert_to_json(obj.schedule_version)).value();
         result += ",\"producer_signature\":" + eosio::check(eosio::convert_to_json(obj.producer_signature)).value();
         result += ",\"ref_block_prefix\":" + eosio::check(eosio::convert_to_json(ref_block_prefix)).value();
         result += "}";

         thread_state.action_return_value.assign(result.data(), result.data() + result.size());
         return thread_state.action_return_value;
      }
   }

   throw std::runtime_error("block " + std::to_string(params.block_num_or_id) + " not found");
} // query_get_block

struct get_abi_params {
   abieos::name account_name = {};
};

EOSIO_REFLECT(get_abi_params, account_name)

struct get_abi_result {
   abieos::name                  account_name;
   std::optional<eosio::abi_def> abi;
};

EOSIO_REFLECT(get_abi_result, account_name, abi)

const std::vector<char>& query_get_abi(wasm_ql::thread_state& thread_state, std::string_view body) {
   get_abi_params           params;
   std::string              s{ body.begin(), body.end() };
   eosio::json_token_stream stream{ s.data() };
   if (auto r = from_json(params, stream); !r)
      throw std::runtime_error("An error occurred deserializing get_abi_params: " + r.error().message());

   rocksdb::ManagedSnapshot          snapshot{ thread_state.shared->db->rdb.get() };
   chain_kv::write_session           write_session{ *thread_state.shared->db, snapshot.snapshot() };
   state_history::rdb::db_view_state db_view_state{ abieos::name{ "state" }, *thread_state.shared->db, write_session };

   auto acc = get_state_row<state_history::account>(
         db_view_state.kv_state.view,
         std::make_tuple(abieos::name{ "account" }, abieos::name{ "primary" }, params.account_name));
   if (!acc)
      throw std::runtime_error("account " + (std::string)params.account_name + " not found");
   auto& acc0 = std::get<state_history::account_v0>(acc->second);

   get_abi_result result;
   result.account_name = acc0.name;
   if (acc0.abi.pos != acc0.abi.end) {
      result.abi.emplace();
      eosio::check_discard(eosio::from_bin(*result.abi, acc0.abi));
   }

   // todo: avoid the extra copy
   auto json = eosio::check(eosio::convert_to_json(result));
   thread_state.action_return_value.assign(json.value().begin(), json.value().end());
   return thread_state.action_return_value;
} // query_get_abi

// Ignores data field
struct action_no_data {
   abieos::name                                 account       = {};
   abieos::name                                 name          = {};
   std::vector<state_history::permission_level> authorization = {};
};

struct extension_hex_data {
   uint16_t  type = {};
   hex_bytes data = {};
};

EOSIO_REFLECT(extension_hex_data, type, data)

EOSIO_REFLECT(action_no_data, account, name, authorization)

struct transaction_for_get_keys : state_history::transaction_header {
   std::vector<action_no_data>     context_free_actions   = {};
   std::vector<action_no_data>     actions                = {};
   std::vector<extension_hex_data> transaction_extensions = {};
};

EOSIO_REFLECT(transaction_for_get_keys, base state_history::transaction_header, context_free_actions, actions,
              transaction_extensions)

struct get_required_keys_params {
   transaction_for_get_keys        transaction    = {};
   std::vector<abieos::public_key> available_keys = {};
};

EOSIO_REFLECT(get_required_keys_params, transaction, available_keys)

struct get_required_keys_result {
   std::vector<abieos::public_key> required_keys = {};
};

EOSIO_REFLECT(get_required_keys_result, required_keys)

const std::vector<char>& query_get_required_keys(wasm_ql::thread_state& thread_state, std::string_view body) {
   get_required_keys_params params;
   std::string              s{ body.begin(), body.end() };
   eosio::json_token_stream stream{ s.data() };
   if (auto r = from_json(params, stream); !r)
      throw std::runtime_error("An error occurred deserializing get_required_keys_params: " + r.error().message());

   get_required_keys_result result;
   for (auto& action : params.transaction.context_free_actions)
      if (!action.authorization.empty())
         throw std::runtime_error("Context-free actions may not have authorizations");
   for (auto& action : params.transaction.actions)
      if (!action.authorization.empty())
         throw std::runtime_error("Actions may not have authorizations"); // todo

   // todo: avoid the extra copy
   auto json = eosio::check(eosio::convert_to_json(result));
   thread_state.action_return_value.assign(json.value().begin(), json.value().end());
   return thread_state.action_return_value;
} // query_get_required_keys

struct send_transaction_params {
   std::vector<abieos::signature> signatures               = {};
   std::string                    compression              = {};
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
      std::string              s{ body.begin(), body.end() };
      eosio::json_token_stream stream{ s.data() };
      if (auto r = from_json(trx, stream); !r)
         throw std::runtime_error("An error occurred deserializing send_transaction_params: "s + r.error().message());
   }
   eosio::input_stream s{ trx.packed_trx.data };
   auto                r = eosio::from_bin<state_history::transaction>(s);
   if (!r)
      throw std::runtime_error("An error occurred deserializing packed_trx: "s + r.error().message());
   if (s.end != s.pos)
      throw std::runtime_error("Extra data in packed_trx");
   state_history::transaction& unpacked = r.value();

   if (!trx.signatures.empty())
      throw std::runtime_error("Signatures must be empty"); // todo
   if (trx.compression != "0" && trx.compression != "none")
      throw std::runtime_error("Compression must be 0 or none"); // todo
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
   for (auto& action : unpacked.actions)
      if (!action.authorization.empty())
         throw std::runtime_error("authorization must be empty"); // todo

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

      try {
         run_query(thread_state, action, at);
      } catch (std::exception& e) {
         // todo: errorcode
         at.except = tt.except = e.what();
         tt.status             = state_history::transaction_status::soft_fail;
         break;
      }
   }

   // todo: avoid the extra copy
   auto json = eosio::check(eosio::convert_to_json(results));
   thread_state.action_return_value.assign(json.value().begin(), json.value().end());
   return thread_state.action_return_value;
} // query_send_transaction

} // namespace wasm_ql
