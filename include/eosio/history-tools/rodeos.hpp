#include <chain_kv/chain_kv.hpp>

namespace eosio { namespace history_tools {

struct rodeos_context {
   std::shared_ptr<chain_kv::database> db;
};

struct rodeos_db_partition {
   //
};

struct rodeos_db_snapshot {
   //
};

}} // namespace eosio::history_tools
