#include "state_history_rocksdb.hpp"

namespace eosio {
using state_history::rdb::kv_environment;
}
#include "../wasms/state_history_kv_tables.hpp" // todo: move

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
