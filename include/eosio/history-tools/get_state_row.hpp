#pragma once

#include <eosio/history-tools/callbacks/kv.hpp>

namespace eosio {
using history_tools::kv_environment;
}
#include <eosio/history-tools/state_history_kv_tables.hpp>

template <typename T, typename K>
std::optional<std::pair<std::shared_ptr<const chain_kv::bytes>, T>> get_state_row(chain_kv::view& view, const K& key) {
   std::optional<std::pair<std::shared_ptr<const chain_kv::bytes>, T>> result;
   result.emplace();
   result->first =
         view.get(eosio::name{ "state" }.value, chain_kv::to_slice(eosio::check(eosio::convert_to_key(std::make_tuple(0x01, key))).value()));
   if (!result->first) {
      result.reset();
      return result;
   }

   eosio::input_stream stream{ *result->first };
   if (auto r = from_bin(result->second, stream); !r)
      throw std::runtime_error("An error occurred deserializing state: " + r.error().message());
   return result;
}

template <typename T, typename K>
std::optional<std::pair<std::shared_ptr<const chain_kv::bytes>, T>> get_state_row_secondary(chain_kv::view& view,
                                                                                            const K&        key) {
   std::optional<std::pair<std::shared_ptr<const chain_kv::bytes>, T>> result;
   auto                                                                pk =
         view.get(eosio::name{ "state" }.value, chain_kv::to_slice(eosio::check(eosio::convert_to_key(std::make_tuple(0x01, key))).value()));
   if (!pk)
      return result;

   result.emplace();
   result->first = view.get(eosio::name{ "state" }.value, chain_kv::to_slice(*pk));
   if (!result->first) {
      result.reset();
      return result;
   }

   eosio::input_stream stream{ *result->first };
   if (auto r = from_bin(result->second, stream); !r)
      throw std::runtime_error("An error occurred deserializing state: " + r.error().message());
   return result;
}