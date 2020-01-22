#include "state_history_connection.hpp"

#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <fc/exception/exception.hpp>

struct state : state_history::connection_callbacks {
    uint32_t                   start_block;
    uint32_t                   end_block;
    state_history::connection* conn;

    void received_abi() override { conn->send(state_history::get_status_request_v0{}); }

    bool received(state_history::get_status_result_v0& status, eosio::input_stream) override {
        ilog("head                      ${h}", ("h", status.head.block_num));
        ilog("last_irreversible         ${h}", ("h", status.last_irreversible.block_num));
        ilog("trace_begin_block         ${x}", ("x", status.trace_begin_block));
        ilog("trace_end_block           ${x}", ("x", status.trace_end_block));
        ilog("chain_state_begin_block   ${x}", ("x", status.chain_state_begin_block));
        ilog("chain_state_end_block     ${x}", ("x", status.chain_state_end_block));

        state_history::get_blocks_request_v0 req;
        req.start_block_num        = start_block;
        req.end_block_num          = end_block;
        req.max_messages_in_flight = 0xffff'ffff;
        req.irreversible_only      = true;
        req.fetch_traces           = true;
        conn->send(req);

        return true;
    }

    bool received(state_history::get_blocks_result_v0& result, eosio::input_stream) override {
        if (!result.this_block || !result.traces)
            return false;
        auto     bin = *result.traces;
        uint32_t num_ttraces;
        eosio::check_discard(eosio::varuint32_from_bin(num_ttraces, bin));

        uint64_t num_atraces = 0;
        for (uint32_t i = 0; i < num_ttraces; ++i) {
            state_history::transaction_trace trace;
            eosio::check_discard(from_bin(trace, bin));
            std::visit([&](auto& trace) { num_atraces += trace.action_traces.size(); }, trace);
        }

        ilog("block: ${b}, transactions: ${t}, actions: ${a}", ("b", result.this_block->block_num)("t", num_ttraces)("a", num_atraces));

        return result.this_block->block_num + 1 < end_block;
    }

    void closed(bool retry) override{};
};

enum return_codes {
    other_fail      = -2,
    initialize_fail = -1,
    success         = 0,
    bad_alloc       = 1,
};

int main(int argc, char** argv) {
    try {
        if (argc < 5)
            throw std::runtime_error("usage: dump-info host port start-block end-block");
        boost::asio::io_context          ioc;
        state_history::connection_config config{argv[1], argv[2]};
        auto                             cb = std::make_shared<state>();
        cb->start_block                     = std::stoul(argv[3]);
        cb->end_block                       = std::stoul(argv[4]);
        auto conn                           = std::make_shared<state_history::connection>(ioc, config, cb);
        cb->conn                            = conn.get();
        conn->connect();
        ioc.run();
    } catch (const fc::exception& e) {
        elog("${e}", ("e", e.to_detail_string()));
        return other_fail;
    } catch (const boost::interprocess::bad_alloc& e) {
        elog("bad alloc");
        return bad_alloc;
    } catch (const boost::exception& e) {
        elog("${e}", ("e", boost::diagnostic_information(e)));
        return other_fail;
    } catch (const std::exception& e) {
        elog("${e}", ("e", e.what()));
        return other_fail;
    } catch (...) {
        elog("unknown exception");
        return other_fail;
    }

    return success;
}
