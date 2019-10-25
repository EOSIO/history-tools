// copyright defined in LICENSE.txt

#include "fill_rocksdb_plugin.hpp"
#include "state_history_connection.hpp"
#include "state_history_kv.hpp"
#include "state_history_rocksdb.hpp"
#include "util.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <fc/exception/exception.hpp>

using namespace appbase;
using namespace std::literals;
using namespace state_history;

namespace asio      = boost::asio;
namespace bpo       = boost::program_options;
namespace kv        = state_history::kv;
namespace rdb       = state_history::rdb;
namespace websocket = boost::beast::websocket;

using asio::ip::tcp;
using boost::beast::flat_buffer;
using boost::system::error_code;

using abieos::abi_type;
using abieos::checksum256;
using abieos::input_buffer;

struct flm_session;

struct fill_rocksdb_config : connection_config {
    uint32_t                skip_to     = 0;
    uint32_t                stop_before = 0;
    std::vector<trx_filter> trx_filters = {};
};

struct fill_rocksdb_plugin_impl : std::enable_shared_from_this<fill_rocksdb_plugin_impl> {
    std::shared_ptr<fill_rocksdb_config> config = std::make_shared<fill_rocksdb_config>();
    std::shared_ptr<::flm_session>       session;
    boost::asio::deadline_timer          timer;

    fill_rocksdb_plugin_impl()
        : timer(app().get_io_service()) {}

    ~fill_rocksdb_plugin_impl();

    void schedule_retry() {
        timer.expires_from_now(boost::posix_time::seconds(1));
        timer.async_wait([this](auto&) {
            ilog("retry...");
            start();
        });
    }

    void start();
};

struct flm_session : connection_callbacks, std::enable_shared_from_this<flm_session> {
    fill_rocksdb_plugin_impl*                     my = nullptr;
    std::shared_ptr<fill_rocksdb_config>          config;
    std::shared_ptr<state_history::rdb::database> db = app().find_plugin<rocksdb_plugin>()->get_db();
    rocksdb::WriteBatch                           active_content_batch;
    rocksdb::WriteBatch                           active_index_batch;
    std::shared_ptr<state_history::connection>    connection;
    std::optional<state_history::fill_status>     current_db_status = {};
    uint32_t                                      head              = 0;
    abieos::checksum256                           head_id           = {};
    uint32_t                                      irreversible      = 0;
    abieos::checksum256                           irreversible_id   = {};
    uint32_t                                      first             = 0;

    flm_session(fill_rocksdb_plugin_impl* my)
        : my(my)
        , config(my->config) {}

    void connect(asio::io_context& ioc) {
        connection = std::make_shared<state_history::connection>(ioc, *config, shared_from_this());
        connection->connect();
    }

    void received_abi(std::string_view abi) override {
        load_fill_status();
        ilog("clean up stale records");
        end_write(true);
        // truncate(head + 1);
        end_write(true);
        db->flush(true, true);

        ilog("request status");
        connection->send(get_status_request_v0{});
    }

    bool received(get_status_result_v0& status, abieos::input_buffer bin) override {
        ilog("request blocks");
        connection->request_blocks(status, std::max(config->skip_to, head + 1), get_positions());
        return true;
    }

    void load_fill_status() {
        current_db_status = rdb::get<state_history::fill_status>(*db, kv::make_fill_status_key(), false);
        if (!current_db_status)
            return;
        head            = current_db_status->head;
        head_id         = current_db_status->head_id;
        irreversible    = current_db_status->irreversible;
        irreversible_id = current_db_status->irreversible_id;
        first           = current_db_status->first;
    }

    std::vector<block_position> get_positions() {
        std::vector<block_position> result;
        if (head) {
            for (uint32_t i = irreversible; i <= head; ++i) {
                auto rb = rdb::get<kv::received_block>(*db, kv::make_received_block_key(i), true);
                result.push_back({rb->block_num, rb->block_id});
            }
        }
        return result;
    }

    void write_fill_status(rocksdb::WriteBatch& batch) {
        if (irreversible < head)
            current_db_status = state_history::fill_status{
                .head = head, .head_id = head_id, .irreversible = irreversible, .irreversible_id = irreversible_id, .first = first};
        else
            current_db_status = state_history::fill_status{
                .head = head, .head_id = head_id, .irreversible = head, .irreversible_id = head_id, .first = first};
        rdb::put(batch, kv::make_fill_status_key(), *current_db_status, true);
    }

    void end_write(bool write_fill) {
        if (write_fill)
            write_fill_status(active_index_batch);

        // write content before indexes to enable truncate() to behave correctly if process exits before flushing
        write(*db, active_content_batch);
        write(*db, active_index_batch);
    }

    bool received(get_blocks_result_v0& result, abieos::input_buffer bin) override {
        if (!result.this_block)
            return true;
        if (config->stop_before && result.this_block->block_num >= config->stop_before) {
            ilog("block ${b}: stop requested", ("b", result.this_block->block_num));
            end_write(true);
            db->flush(false, false);
            return false;
        }

        /*
        if (result.this_block->block_num >= 40013524) {
            end_write(true);
            {
                std::string ss;
                db->db->GetProperty("rocksdb.stats", &ss);
                std::cout << ss << "\n";
            }
            db->flush(true, true);
            {
                std::string ss;
                db->db->GetProperty("rocksdb.stats", &ss);
                std::cout << ss << "\n";
            }
            _exit(0);
        }
        */

        try {
            if (result.this_block->block_num <= head) {
                ilog("switch forks at block ${b}", ("b", result.this_block->block_num));
                end_write(true);
                throw std::runtime_error("truncate not implemented");
                // truncate(result.this_block->block_num);
                end_write(true);
            }

            bool near       = result.this_block->block_num + 4 >= result.last_irreversible.block_num;
            bool commit_now = !(result.this_block->block_num % 200) || near;
            if (commit_now)
                ilog("block ${b}", ("b", result.this_block->block_num));

            if (head_id != abieos::checksum256{} && (!result.prev_block || result.prev_block->block_id != head_id))
                throw std::runtime_error("prev_block does not match");
            if (result.block)
                receive_block(
                    result.this_block->block_num, result.this_block->block_id, *result.block, active_content_batch, active_index_batch);
            if (result.deltas)
                receive_deltas(active_content_batch, active_index_batch, result.this_block->block_num, *result.deltas);
            if (result.traces)
                receive_traces(active_content_batch, active_index_batch, result.this_block->block_num, *result.traces);

            head            = result.this_block->block_num;
            head_id         = result.this_block->block_id;
            irreversible    = result.last_irreversible.block_num;
            irreversible_id = result.last_irreversible.block_id;
            if (!first)
                first = head;

            rdb::put(
                active_content_batch, kv::make_received_block_key(result.this_block->block_num),
                kv::received_block{result.this_block->block_num, result.this_block->block_id});

            if (commit_now) {
                end_write(true);
            }
            if (near)
                db->flush(false, false);
        } catch (...) {
            throw;
        }

        return true;
    } // receive_result()

    void receive_block(
        uint32_t block_num, const checksum256& block_id, input_buffer bin, rocksdb::WriteBatch& content_batch,
        rocksdb::WriteBatch& index_batch) {
        state_history::signed_block block;
        bin_to_native(block, bin);
        std::vector<char> value;

        abieos::native_to_bin(block_num, value);
        abieos::native_to_bin(block_id, value);
        abieos::native_to_bin(block.timestamp, value);
        abieos::native_to_bin(block.producer, value);
        abieos::native_to_bin(block.confirmed, value);
        abieos::native_to_bin(block.previous, value);
        abieos::native_to_bin(block.transaction_mroot, value);
        abieos::native_to_bin(block.action_mroot, value);
        abieos::native_to_bin(block.schedule_version, value);
        abieos::native_to_bin(block.new_producers ? *block.new_producers : state_history::producer_schedule{}, value);

        // add_row(content_batch, index_batch, get_table("block_info"), block_num, true, value);
    } // receive_block

    void receive_deltas(rocksdb::WriteBatch& content_batch, rocksdb::WriteBatch& index_batch, uint32_t block_num, input_buffer bin) {
        auto&             table_delta_type = get_type("table_delta");
        std::vector<char> value;
        /*
        auto num = read_varuint32(bin);
        for (uint32_t i = 0; i < num; ++i) {
            check_variant(bin, table_delta_type, "table_delta_v0");
            state_history::table_delta_v0 table_delta;
            bin_to_native(table_delta, bin);
            auto& table = get_table(table_delta.name);

            size_t num_processed = 0;
            for (auto& row : table_delta.rows) {
                if (table_delta.rows.size() > 10000 && !(num_processed % 10000)) {
                    ilog(
                        "block ${b} ${t} ${n} of ${r}",
                        ("b", block_num)("t", table_delta.name)("n", num_processed)("r", table_delta.rows.size()));
                    end_write(false);
                }
                check_variant(row.data, *table.abi_type, 0u);
                value.clear();
                abieos::native_to_bin(block_num, value);
                abieos::native_to_bin(row.present, value);
                for (auto& field : table.fields)
                    fill(value, row.data, *field);
                add_row(content_batch, index_batch, table, block_num, row.present, value);
                ++num_processed;
            }
        }
        */
    } // receive_deltas

    void receive_traces(rocksdb::WriteBatch& content_batch, rocksdb::WriteBatch& index_batch, uint32_t block_num, input_buffer bin) {
        auto     num          = read_varuint32(bin);
        uint32_t num_ordinals = 0;
        for (uint32_t i = 0; i < num; ++i) {
            state_history::transaction_trace trace;
            bin_to_native(trace, bin);
            if (filter(config->trx_filters, std::get<0>(trace)))
                write_transaction_trace(
                    content_batch, index_batch, block_num, num_ordinals, std::get<state_history::transaction_trace_v0>(trace));
        }
    }

    void write_transaction_trace(
        rocksdb::WriteBatch& content_batch, rocksdb::WriteBatch& index_batch, uint32_t block_num, uint32_t& num_ordinals,
        const state_history::transaction_trace_v0& ttrace) {
        auto* failed = !ttrace.failed_dtrx_trace.empty()
                           ? &std::get<state_history::transaction_trace_v0>(ttrace.failed_dtrx_trace[0].recurse)
                           : nullptr;
        if (failed) {
            if (!filter(config->trx_filters, *failed))
                return;
            write_transaction_trace(content_batch, index_batch, block_num, num_ordinals, *failed);
        }
        uint32_t transaction_ordinal = ++num_ordinals;

        std::vector<char> key;
        kv::append_transaction_trace_key(key, block_num, ttrace.id);

        std::vector<char> value;
        abieos::native_to_bin(block_num, value);
        abieos::native_to_bin(transaction_ordinal, value);
        abieos::native_to_bin(failed ? failed->id : abieos::checksum256{}, value);
        abieos::native_to_bin(ttrace.id, value);
        abieos::native_to_bin((uint8_t)ttrace.status, value);
        abieos::native_to_bin(ttrace.cpu_usage_us, value);
        abieos::native_to_bin(ttrace.net_usage_words, value);
        abieos::native_to_bin(ttrace.elapsed, value);
        abieos::native_to_bin(ttrace.net_usage, value);
        abieos::native_to_bin(ttrace.scheduled, value);
        abieos::native_to_bin(ttrace.account_ram_delta.has_value(), value);
        if (ttrace.account_ram_delta) {
            abieos::native_to_bin(ttrace.account_ram_delta->account, value);
            abieos::native_to_bin(ttrace.account_ram_delta->delta, value);
        }
        abieos::native_to_bin(ttrace.except ? *ttrace.except : "", value);
        abieos::native_to_bin(ttrace.error_code ? *ttrace.error_code : 0, value);

        // rdb::put(batch, key, value); // todo: indexes, including trim

        for (auto& atrace : ttrace.action_traces)
            write_action_trace(content_batch, index_batch, block_num, ttrace, std::get<state_history::action_trace_v0>(atrace), value);
    }

    void write_action_trace(
        rocksdb::WriteBatch& content_batch, rocksdb::WriteBatch& index_batch, uint32_t block_num,
        const state_history::transaction_trace_v0& ttrace, const state_history::action_trace_v0& atrace, std::vector<char>& value) {
        /*
        value.clear();

        abieos::native_to_bin(block_num, value);
        abieos::native_to_bin(ttrace.id, value);
        abieos::native_to_bin((uint8_t)ttrace.status, value);
        abieos::native_to_bin(atrace.action_ordinal, value);
        abieos::native_to_bin(atrace.creator_action_ordinal, value);
        abieos::native_to_bin(atrace.receipt.has_value(), value);
        if (atrace.receipt) {
            auto& receipt = std::get<state_history::action_receipt_v0>(*atrace.receipt);
            abieos::native_to_bin(receipt.receiver, value);
            abieos::native_to_bin(receipt.act_digest, value);
            abieos::native_to_bin(receipt.global_sequence, value);
            abieos::native_to_bin(receipt.recv_sequence, value);
            abieos::native_to_bin(receipt.code_sequence, value);
            abieos::native_to_bin(receipt.abi_sequence, value);
        }
        abieos::native_to_bin(atrace.receiver, value);
        abieos::native_to_bin(atrace.act.account, value);
        abieos::native_to_bin(atrace.act.name, value);
        abieos::native_to_bin(atrace.act.data, value);
        abieos::native_to_bin(atrace.context_free, value);
        abieos::native_to_bin(atrace.elapsed, value);
        abieos::native_to_bin(atrace.console, value);
        abieos::native_to_bin(atrace.except ? *atrace.except : "", value);
        abieos::native_to_bin(atrace.error_code ? *atrace.error_code : 0, value);

        abieos::native_to_bin(atrace.console, value);
        abieos::native_to_bin(atrace.except ? *atrace.except : std::string(), value);

        add_row(content_batch, index_batch, get_table("action_trace"), block_num, true, value);
        */

        // todo: receipt_auth_sequence
        // todo: authorization
        // todo: account_ram_deltas
    }

    const abi_type& get_type(const std::string& name) { return connection->get_type(name); }

    void closed(bool retry) override {
        if (my) {
            my->session.reset();
            if (retry)
                my->schedule_retry();
        }
    }

    ~flm_session() {}
}; // flm_session

static abstract_plugin& _fill_rocksdb_plugin = app().register_plugin<fill_rocksdb_plugin>();

fill_rocksdb_plugin_impl::~fill_rocksdb_plugin_impl() {
    if (session)
        session->my = nullptr;
}

void fill_rocksdb_plugin_impl::start() {
    session = std::make_shared<flm_session>(this);
    session->connect(app().get_io_service());
}

fill_rocksdb_plugin::fill_rocksdb_plugin()
    : my(std::make_shared<fill_rocksdb_plugin_impl>()) {}

fill_rocksdb_plugin::~fill_rocksdb_plugin() {}

void fill_rocksdb_plugin::set_program_options(options_description& cli, options_description& cfg) { auto clop = cli.add_options(); }

void fill_rocksdb_plugin::plugin_initialize(const variables_map& options) {
    try {
        auto endpoint = options.at("fill-connect-to").as<std::string>();
        if (endpoint.find(':') == std::string::npos)
            throw std::runtime_error("invalid endpoint: " + endpoint);

        auto port               = endpoint.substr(endpoint.find(':') + 1, endpoint.size());
        auto host               = endpoint.substr(0, endpoint.find(':'));
        my->config->host        = host;
        my->config->port        = port;
        my->config->skip_to     = options.count("fill-skip-to") ? options["fill-skip-to"].as<uint32_t>() : 0;
        my->config->stop_before = options.count("fill-stop") ? options["fill-stop"].as<uint32_t>() : 0;
        my->config->trx_filters = fill_plugin::get_trx_filters(options);
    }
    FC_LOG_AND_RETHROW()
}

void fill_rocksdb_plugin::plugin_startup() { my->start(); }

void fill_rocksdb_plugin::plugin_shutdown() {
    if (my->session)
        my->session->connection->close(false);
    my->timer.cancel();
    ilog("fill_rocksdb_plugin stopped");
}
