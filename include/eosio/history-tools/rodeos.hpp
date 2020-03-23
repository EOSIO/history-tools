#include <chain_kv/chain_kv.hpp>
#include <eosio/history-tools/filter.hpp>
#include <eosio/history-tools/wasm_ql.hpp>
#include <eosio/ship_protocol.hpp>

namespace eosio { namespace history_tools {

static constexpr char undo_prefix_byte        = 0x01;
static constexpr char contract_kv_prefix_byte = 0x02;

struct rodeos_context {
   std::shared_ptr<chain_kv::database> db;
};

struct rodeos_db_partition {
   const std::shared_ptr<chain_kv::database> db;
   const std::vector<char>                   undo_prefix;
   const std::vector<char>                   contract_kv_prefix;

   // todo: move rocksdb::ManagedSnapshot to here to prevent optimization in cloner from
   //       defeating non-persistent snapshots.

   rodeos_db_partition(std::shared_ptr<chain_kv::database> db, const std::vector<char>& prefix)
       : db{ std::move(db) }, //
         undo_prefix{ [&] {
            auto x = prefix;
            x.push_back(undo_prefix_byte);
            return x;
         }() },
         contract_kv_prefix{ [&] {
            auto x = prefix;
            x.push_back(contract_kv_prefix_byte);
            return x;
         }() } {}
};

struct rodeos_db_snapshot {
   std::shared_ptr<rodeos_db_partition>    partition       = {};
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
   std::optional<uint32_t>                 writing_block   = {};

   rodeos_db_snapshot(std::shared_ptr<rodeos_db_partition> partition, bool persistent);

   void refresh();
   void end_write(bool write_fill);
   void start_block(const ship_protocol::get_blocks_result_v0& result);
   void end_block(const ship_protocol::get_blocks_result_v0& result, bool force_write);
   void check_write(const ship_protocol::get_blocks_result_v0& result);
   void write_deltas(const ship_protocol::get_blocks_result_v0& result, std::function<bool()> shutdown);

 private:
   void write_fill_status();
};

struct rodeos_filter {
   std::unique_ptr<filter::backend_t>    backend      = {};
   std::unique_ptr<filter::filter_state> filter_state = {};

   rodeos_filter(const std::string filter_wasm);

   void process(rodeos_db_snapshot& snapshot, const ship_protocol::get_blocks_result_v0& result,
                eosio::input_stream bin);
};

struct rodeos_query_handler {
   std::shared_ptr<rodeos_db_partition>               partition;
   const std::shared_ptr<const wasm_ql::shared_state> shared_state;
   wasm_ql::thread_state_cache                        state_cache;

   rodeos_query_handler(std::shared_ptr<rodeos_db_partition>         partition,
                        std::shared_ptr<const wasm_ql::shared_state> shared_state);
   rodeos_query_handler(const rodeos_query_handler&) = delete;
};

}} // namespace eosio::history_tools
