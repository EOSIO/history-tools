#include <chain_kv/chain_kv.hpp>

namespace eosio { namespace history_tools {

static constexpr char undo_prefix = 0x01;

struct rodeos_context {
   std::shared_ptr<chain_kv::database> db;
};

struct rodeos_db_partition {
   std::shared_ptr<chain_kv::database> db;
   std::vector<char>                   prefix;

   // todo: move rocksdb::ManagedSnapshot to here to prevent optimization in cloner from
   //       defeating non-persistent snapshots.

   rodeos_db_partition(std::shared_ptr<chain_kv::database> db, std::vector<char> prefix)
       : db{ std::move(db) }, prefix{ std::move(prefix) } {}
};

struct rodeos_db_snapshot {
   std::shared_ptr<chain_kv::database>     db;
   std::optional<chain_kv::undo_stack>     undo_stack; // only if persistent
   std::optional<rocksdb::ManagedSnapshot> snap;       // only if !persistent
   std::optional<chain_kv::write_session>  write_session;

   rodeos_db_snapshot(rodeos_db_partition& partition, bool persistent) : db{ partition.db } {
      if (persistent) {
         auto p = partition.prefix;
         p.push_back(undo_prefix);
         undo_stack.emplace(*db, std::move(p));
         write_session.emplace(*db);
      } else {
         snap.emplace(db->rdb.get());
         write_session.emplace(*db, snap->snapshot());
      }
   }
};

}} // namespace eosio::history_tools
