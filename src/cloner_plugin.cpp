#include "cloner_plugin.hpp"
#include "filter.hpp"
#include "get_state_row.hpp"
#include "state_history_connection.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <fc/exception/exception.hpp>

using namespace appbase;
using namespace std::literals;
using namespace eosio::ship_protocol;

namespace asio          = boost::asio;
namespace bpo           = boost::program_options;
namespace filter        = eosio::history_tools::filter;
namespace history_tools = eosio::history_tools;
namespace ship_protocol = eosio::ship_protocol;
namespace websocket     = boost::beast::websocket;

using asio::ip::tcp;
using boost::beast::flat_buffer;
using boost::system::error_code;

struct cloner_session;

struct cloner_config : connection_config {
   uint32_t    skip_to     = 0;
   uint32_t    stop_before = 0;
   std::string filter_wasm = {}; // todo: remove
};

struct cloner_plugin_impl : std::enable_shared_from_this<cloner_plugin_impl> {
   std::shared_ptr<cloner_config>    config = std::make_shared<cloner_config>();
   std::shared_ptr<::cloner_session> session;
   boost::asio::deadline_timer       timer;

   cloner_plugin_impl() : timer(app().get_io_service()) {}

   ~cloner_plugin_impl();

   void schedule_retry() {
      timer.expires_from_now(boost::posix_time::seconds(1));
      timer.async_wait([this](auto&) {
         ilog("retry...");
         start();
      });
   }

   void start();
};

struct cloner_session : connection_callbacks, std::enable_shared_from_this<cloner_session> {
   cloner_plugin_impl*                        my = nullptr;
   std::shared_ptr<cloner_config>             config;
   std::shared_ptr<chain_kv::database>        db = app().find_plugin<rocksdb_plugin>()->get_db();
   chain_kv::undo_stack                       undo_stack{ *db, chain_kv::bytes{ history_tools::undo_stack_prefix } };
   chain_kv::write_session                    write_session{ *db };
   std::shared_ptr<ship_protocol::connection> connection;
   eosio::checksum256                         chain_id        = {};
   uint32_t                                   head            = 0;
   eosio::checksum256                         head_id         = {};
   uint32_t                                   irreversible    = 0;
   eosio::checksum256                         irreversible_id = {};
   uint32_t                                   first           = 0;
   bool                                       reported_block  = false;
   std::unique_ptr<filter::backend_t>         backend         = {}; // todo: remove
   std::unique_ptr<filter::filter_state>      filter_state    = {}; // todo: remove

   cloner_session(cloner_plugin_impl* my) : my(my), config(my->config) {
      // todo: remove
      if (!config->filter_wasm.empty()) {
         std::ifstream wasm_file(config->filter_wasm, std::ios::binary);
         if (!wasm_file.is_open())
            throw std::runtime_error("can not open " + config->filter_wasm);
         ilog("compiling ${f}", ("f", config->filter_wasm));
         wasm_file.seekg(0, std::ios::end);
         int len = wasm_file.tellg();
         if (len < 0)
            throw std::runtime_error("wasm file length is -1");
         std::vector<uint8_t> code(len);
         wasm_file.seekg(0, std::ios::beg);
         wasm_file.read((char*)code.data(), code.size());
         wasm_file.close();
         backend      = std::make_unique<filter::backend_t>(code);
         filter_state = std::make_unique<filter::filter_state>();
         filter::rhf_t::resolve(backend->get_module());
      }
   }

   void connect(asio::io_context& ioc) {
      load_fill_status();
      end_write(true);
      db->flush(true, true);

      connection = std::make_shared<ship_protocol::connection>(ioc, *config, shared_from_this());
      connection->connect();
   }

   void received_abi() override {
      ilog("request status");
      connection->send(get_status_request_v0{});
   }

   bool received(get_status_result_v0& status, eosio::input_stream bin) override {
      ilog("nodeos has chain ${c}", ("c", eosio::check(eosio::convert_to_json(status.chain_id)).value()));
      if (chain_id == eosio::checksum256{})
         chain_id = status.chain_id;
      if (chain_id != status.chain_id)
         throw std::runtime_error("database is for chain " + eosio::check(eosio::convert_to_json(chain_id)).value() +
                                  " but nodeos has chain " +
                                  eosio::check(eosio::convert_to_json(status.chain_id)).value());
      ilog("request blocks");
      connection->request_blocks(status, std::max(config->skip_to, head + 1), get_positions());
      return true;
   }

   void load_fill_status() {
      write_session.wipe_cache();
      history_tools::db_view_state view_state{ eosio::name{ "state" }, *db, write_session };
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
      ilog("cloner database status:");
      ilog("    revisions:    ${f} - ${r}", ("f", undo_stack.first_revision())("r", undo_stack.revision()));
      ilog("    chain:        ${a}", ("a", eosio::check(eosio::convert_to_json(chain_id)).value()));
      ilog("    head:         ${a} ${b}", ("a", head)("b", eosio::check(eosio::convert_to_json(head_id)).value()));
      ilog("    irreversible: ${a} ${b}",
           ("a", irreversible)("b", eosio::check(eosio::convert_to_json(irreversible_id)).value()));
   }

   std::vector<block_position> get_positions() {
      std::vector<block_position> result;
      if (head) {
         history_tools::db_view_state view_state{ eosio::name{ "state" }, *db, write_session };
         for (uint32_t i = irreversible; i <= head; ++i) {
            auto info = get_state_row<block_info>(
                  view_state.kv_state.view, std::make_tuple(eosio::name{ "block.info" }, eosio::name{ "primary" }, i));
            if (!info)
               throw std::runtime_error("database is missing block.info for block " + std::to_string(i));
            auto& info0 = std::get<block_info_v0>(info->second);
            result.push_back({ info0.num, info0.id });
         }
      }
      return result;
   }

   void write_fill_status() {
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

      history_tools::db_view_state view_state{ eosio::name{ "state" }, *db, write_session };
      fill_status_kv               table{ { view_state } };
      table.insert(status);
   }

   void end_write(bool write_fill) {
      if (write_fill)
         write_fill_status();
      write_session.write_changes(undo_stack);
   }

   bool received(get_blocks_result_v0& result, eosio::input_stream bin) override {
      if (!result.this_block)
         return true;
      if (config->stop_before && result.this_block->block_num >= config->stop_before) {
         ilog("block ${b}: stop requested", ("b", result.this_block->block_num));
         end_write(true);
         db->flush(false, false);
         return false;
      }
      if (head && result.this_block->block_num > head + 1)
         throw std::runtime_error("state-history plugin is missing block " + std::to_string(head + 1));

      if (result.this_block->block_num <= head) {
         ilog("switch forks at block ${b}; database contains revisions ${f} - ${h}",
              ("b", result.this_block->block_num)("f", undo_stack.first_revision())("h", undo_stack.revision()));
         if (undo_stack.first_revision() >= result.this_block->block_num)
            throw std::runtime_error("can't switch forks since database doesn't contain revision " +
                                     std::to_string(result.this_block->block_num - 1));
         write_session.wipe_cache();
         while (undo_stack.revision() >= result.this_block->block_num) //
            undo_stack.undo(true);
         load_fill_status();
         reported_block = false;
      }

      bool near      = result.this_block->block_num + 4 >= result.last_irreversible.block_num;
      bool write_now = !(result.this_block->block_num % 200) || near;
      if (write_now || !reported_block)
         ilog("block ${b} ${i}",
              ("b", result.this_block->block_num)(
                    "i", result.this_block->block_num <= result.last_irreversible.block_num ? "irreversible" : ""));
      reported_block = true;
      if (head_id != eosio::checksum256{} && (!result.prev_block || result.prev_block->block_id != head_id))
         throw std::runtime_error("prev_block does not match");

      if (result.this_block->block_num <= result.last_irreversible.block_num) {
         undo_stack.commit(std::min(result.last_irreversible.block_num, head));
         undo_stack.set_revision(result.this_block->block_num, false);
      } else {
         end_write(false);
         undo_stack.commit(std::min(result.last_irreversible.block_num, head));
         undo_stack.push(false);
      }

      if (result.block)
         receive_block(result.this_block->block_num, result.this_block->block_id, *result.block);
      if (result.deltas)
         receive_deltas(result.this_block->block_num, *result.deltas);

      // todo: remove
      if (backend) {
         history_tools::chaindb_state chaindb_state;
         history_tools::db_view_state view_state{ eosio::name{ "eosio.filter" }, *db, write_session };
         filter::callbacks            cb{ *filter_state, chaindb_state, view_state };
         filter_state->max_console_size = 10000;
         filter_state->console.clear();
         filter_state->input_data = bin;
         backend->set_wasm_allocator(&filter_state->wa);
         backend->initialize(&cb);
         try {
            (*backend)(&cb, "env", "apply", uint64_t(0), uint64_t(0), uint64_t(0));
         } catch (...) {
            if (!filter_state->console.empty())
               ilog("console output before exception:\n${c}", ("c", filter_state->console));
            throw;
         }
         if (!filter_state->console.empty())
            ilog("console output:\n${c}", ("c", filter_state->console));
      }

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
      return true;
   } // receive_result()

   void receive_deltas(uint32_t block_num, eosio::input_stream bin) {
      history_tools::db_view_state view_state{ eosio::name{ "state" }, *db, write_session };
      uint32_t                     num;
      eosio::check_discard(eosio::varuint32_from_bin(num, bin));
      for (uint32_t i = 0; i < num; ++i) {
         table_delta delta;
         eosio::check_discard(from_bin(delta, bin));
         auto&  delta_v0      = std::get<0>(delta);
         size_t num_processed = 0;
         store_delta({ view_state }, delta_v0, head == 0, [&]() {
            if (delta_v0.rows.size() > 10000 && !(num_processed % 10000)) {
               if (app().is_quiting())
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
   } // receive_deltas

   void receive_block(uint32_t block_num, const eosio::checksum256& block_id, eosio::input_stream bin) {
      signed_block block;
      eosio::check_discard(from_bin(block, bin));

      block_info_v0 info;
      info.num                = block_num;
      info.id                 = block_id;
      info.timestamp          = block.timestamp;
      info.producer           = block.producer;
      info.confirmed          = block.confirmed;
      info.previous           = block.previous;
      info.transaction_mroot  = block.transaction_mroot;
      info.action_mroot       = block.action_mroot;
      info.schedule_version   = block.schedule_version;
      info.new_producers      = block.new_producers;
      info.producer_signature = block.producer_signature;

      history_tools::db_view_state view_state{ eosio::name{ "state" }, *db, write_session };
      block_info_kv                table{ { view_state } };
      table.insert(info);
   }

   void closed(bool retry) override {
      if (my) {
         my->session.reset();
         if (retry)
            my->schedule_retry();
      }
   }

   ~cloner_session() {}
}; // cloner_session

static abstract_plugin& _cloner_plugin = app().register_plugin<cloner_plugin>();

cloner_plugin_impl::~cloner_plugin_impl() {
   if (session)
      session->my = nullptr;
}

void cloner_plugin_impl::start() {
   session = std::make_shared<cloner_session>(this);
   session->connect(app().get_io_service());
}

cloner_plugin::cloner_plugin() : my(std::make_shared<cloner_plugin_impl>()) {}

cloner_plugin::~cloner_plugin() {}

void cloner_plugin::set_program_options(options_description& cli, options_description& cfg) {
   auto op   = cfg.add_options();
   auto clop = cli.add_options();
   op("clone-connect-to,f", bpo::value<std::string>()->default_value("127.0.0.1:8080"),
      "State-history endpoint to connect to (nodeos)");
   clop("clone-skip-to,k", bpo::value<uint32_t>(), "Skip blocks before [arg]");
   clop("clone-stop,x", bpo::value<uint32_t>(), "Stop before block [arg]");
   // todo: remove
   op("filter-wasm", bpo::value<std::string>(), "Filter wasm");
}

void cloner_plugin::plugin_initialize(const variables_map& options) {
   try {
      auto endpoint = options.at("clone-connect-to").as<std::string>();
      if (endpoint.find(':') == std::string::npos)
         throw std::runtime_error("invalid endpoint: " + endpoint);

      auto port               = endpoint.substr(endpoint.find(':') + 1, endpoint.size());
      auto host               = endpoint.substr(0, endpoint.find(':'));
      my->config->host        = host;
      my->config->port        = port;
      my->config->skip_to     = options.count("clone-skip-to") ? options["clone-skip-to"].as<uint32_t>() : 0;
      my->config->stop_before = options.count("clone-stop") ? options["clone-stop"].as<uint32_t>() : 0;
      my->config->filter_wasm = options.count("filter-wasm") ? options["filter-wasm"].as<std::string>() : "";

      filter::register_callbacks();
   }
   FC_LOG_AND_RETHROW()
}

void cloner_plugin::plugin_startup() { my->start(); }

void cloner_plugin::plugin_shutdown() {
   if (my->session)
      my->session->connection->close(false);
   my->timer.cancel();
   ilog("cloner_plugin stopped");
}
