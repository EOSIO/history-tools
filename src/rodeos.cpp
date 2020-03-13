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

void rodeos_db_snapshot::start_block(get_blocks_result_v0& result) {
   if (!undo_stack)
      throw std::runtime_error("start_block only works on persistent blocks");
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
}

void rodeos_db_snapshot::end_block(get_blocks_result_v0& result, bool force_write) {
   if (!undo_stack)
      throw std::runtime_error("end_block only works on persistent blocks");
   bool near      = result.this_block->block_num + 4 >= result.last_irreversible.block_num;
   bool write_now = !(result.this_block->block_num % 200) || near || force_write;

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

void rodeos_db_snapshot::write_fill_status() {
   if (!undo_stack)
      throw std::runtime_error("write_fill_status only works on persistent blocks");
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
      throw std::runtime_error("end_write only works on persistent blocks");
   if (write_fill)
      write_fill_status();
   write_session->write_changes(*undo_stack);
}

}} // namespace eosio::history_tools
