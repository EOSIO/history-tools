#include <eosio/history-tools/rodeos.hpp>

#include "state_history_kv_tables.hpp"
#include <eosio/history-tools/callbacks/kv.hpp>

namespace eosio { namespace history_tools {

using ship_protocol::fill_status;
using ship_protocol::fill_status_kv;
using ship_protocol::fill_status_v0;
using ship_protocol::get_blocks_result_v0;

rodeos_db_snapshot::rodeos_db_snapshot(rodeos_db_partition& partition, bool persistent) : db{ partition.db } {
   if (persistent) {
      auto p = partition.prefix;
      p.push_back(undo_prefix);
      undo_stack.emplace(*db, std::move(p));
      write_session.emplace(*db);
   } else {
      snap.emplace(db->rdb.get());
      write_session.emplace(*db, snap->snapshot());
   }

   history_tools::db_view_state view_state{ eosio::name{ "state" }, *db, *write_session };
   fill_status_kv               table{ { view_state } };
   auto                         it = table.begin();
   if (it != table.end()) {
      auto status     = std::get<0>(it.get());
      chain_id        = status.chain_id;
      head            = status.head;
      head_id         = status.head_id;
      irreversible    = status.irreversible;
      irreversible_id = status.irreversible_id;
      first           = status.first;
   }
}

void rodeos_db_snapshot::write_fill_status() {
   if (!undo_stack)
      throw std::runtime_error("Can only write to persistent snapshots");
   fill_status status;
   if (irreversible < head)
      status = fill_status_v0{ .chain_id        = chain_id,
                               .head            = head,
                               .head_id         = head_id,
                               .irreversible    = irreversible,
                               .irreversible_id = irreversible_id,
                               .first           = first };
   else
      status = fill_status_v0{ .chain_id        = chain_id,
                               .head            = head,
                               .head_id         = head_id,
                               .irreversible    = head,
                               .irreversible_id = head_id,
                               .first           = first };

   history_tools::db_view_state view_state{ eosio::name{ "state" }, *db, *write_session };
   fill_status_kv               table{ { view_state } };
   table.insert(status);
}

void rodeos_db_snapshot::end_write(bool write_fill) {
   if (!undo_stack)
      throw std::runtime_error("Can only write to persistent snapshots");
   if (write_fill)
      write_fill_status();
   write_session->write_changes(*undo_stack);
}

void rodeos_db_snapshot::start_block(get_blocks_result_v0& result) {
   if (!undo_stack)
      throw std::runtime_error("Can only write to persistent snapshots");
   if (!result.this_block)
      throw std::runtime_error("get_blocks_result this_block is empty");

   if (result.this_block->block_num <= head) {
      ilog("switch forks at block ${b}; database contains revisions ${f} - ${h}",
           ("b", result.this_block->block_num)("f", undo_stack->first_revision())("h", undo_stack->revision()));
      if (undo_stack->first_revision() >= result.this_block->block_num)
         throw std::runtime_error("can't switch forks since database doesn't contain revision " +
                                  std::to_string(result.this_block->block_num - 1));
      write_session->wipe_cache();
      while (undo_stack->revision() >= result.this_block->block_num) //
         undo_stack->undo(true);
   }

   if (head_id != eosio::checksum256{} && (!result.prev_block || result.prev_block->block_id != head_id))
      throw std::runtime_error("prev_block does not match");

   if (result.this_block->block_num <= result.last_irreversible.block_num) {
      undo_stack->commit(std::min(result.last_irreversible.block_num, head));
      undo_stack->set_revision(result.this_block->block_num, false);
   } else {
      end_write(false);
      undo_stack->commit(std::min(result.last_irreversible.block_num, head));
      undo_stack->push(false);
   }
   writing_block = result.this_block->block_num;
}

void rodeos_db_snapshot::end_block(get_blocks_result_v0& result, bool force_write) {
   if (!undo_stack)
      throw std::runtime_error("Can only write to persistent snapshots");
   if (!result.this_block)
      throw std::runtime_error("get_blocks_result this_block is empty");
   if (!writing_block || result.this_block->block_num != *writing_block)
      throw std::runtime_error("call start_block first");

   bool near       = result.this_block->block_num + 4 >= result.last_irreversible.block_num;
   bool write_now  = !(result.this_block->block_num % 200) || near || force_write;
   head            = result.this_block->block_num;
   head_id         = result.this_block->block_id;
   irreversible    = result.last_irreversible.block_num;
   irreversible_id = result.last_irreversible.block_id;
   if (!first || head < first)
      first = head;
   if (write_now)
      end_write(write_now);
   if (near)
      db->flush(false, false);
}

void rodeos_db_snapshot::write_deltas(ship_protocol::get_blocks_result_v0& result, std::function<bool()> shutdown) {
   if (!undo_stack)
      throw std::runtime_error("Can only write to persistent snapshots");
   if (!result.this_block)
      throw std::runtime_error("get_blocks_result this_block is empty");
   if (!writing_block || result.this_block->block_num != *writing_block)
      throw std::runtime_error("call start_block first");
   if (!result.deltas)
      return;

   uint32_t            block_num = result.this_block->block_num;
   eosio::input_stream bin       = *result.deltas;

   history_tools::db_view_state view_state{ eosio::name{ "state" }, *db, *write_session };
   uint32_t                     num;
   eosio::check_discard(eosio::varuint32_from_bin(num, bin));
   for (uint32_t i = 0; i < num; ++i) {
      ship_protocol::table_delta delta;
      eosio::check_discard(from_bin(delta, bin));
      auto&  delta_v0      = std::get<0>(delta);
      size_t num_processed = 0;
      store_delta({ view_state }, delta_v0, head == 0, [&]() {
         if (delta_v0.rows.size() > 10000 && !(num_processed % 10000)) {
            if (shutdown())
               throw std::runtime_error("shutting down");
            ilog("block ${b} ${t} ${n} of ${r}",
                 ("b", block_num)("t", delta_v0.name)("n", num_processed)("r", delta_v0.rows.size()));
            if (head == 0) {
               end_write(false);
               view_state.reset();
            }
         }
         ++num_processed;
      });
   }
}

}} // namespace eosio::history_tools
