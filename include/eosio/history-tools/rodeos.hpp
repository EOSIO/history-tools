#include <chain_kv/chain_kv.hpp>
#include <eosio/history-tools/ship_protocol.hpp>

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
   std::shared_ptr<chain_kv::database>     db              = {};
   std::optional<chain_kv::undo_stack>     undo_stack      = {}; // only if persistent
   std::optional<rocksdb::ManagedSnapshot> snap            = {}; // only if !persistent
   std::optional<chain_kv::write_session>  write_session   = {};
   eosio::checksum256                      chain_id        = {};
   uint32_t                                head            = 0;
   eosio::checksum256                      head_id         = {};
   uint32_t                                irreversible    = 0;
   eosio::checksum256                      irreversible_id = {};
   uint32_t                                first           = 0;

   rodeos_db_snapshot(rodeos_db_partition& partition, bool persistent);

   void start_block(ship_protocol::get_blocks_result_v0& result);
   void end_block(ship_protocol::get_blocks_result_v0& result, bool force_write);
   void write_fill_status();
   void end_write(bool write_fill);
};

}} // namespace eosio::history_tools
