// copyright defined in LICENSE.txt
#include "parser_plugin.hpp"
#include "state_history_plugin.hpp"
#include <fc/log/logger.hpp>

struct parser_plugin_impl: std::enable_shared_from_this<parser_plugin_impl> {
    std::optional<bsg::scoped_connection>     applied_state_connection;
    std::optional<bsg::scoped_connection>     applied_block_connection;
    
    //datas
    std::optional<state_history::get_status_result_v0> m_status;
    std::optional<state_history::signed_block> m_this_block;
    std::optional<state_history::block_position> m_position;
    uint32_t m_latest_block = 0;
    parser_plugin& m_plugin;

    parser_plugin_impl(parser_plugin& plugin):m_plugin(plugin){}



    void process_block(abieos::input_buffer buffer){
        state_history::signed_block block;
        abieos::bin_to_native(block,buffer);
        m_this_block.emplace(std::move(block));
    }

    void process_traces(abieos::input_buffer buffer){
        uint32_t total_trace = abieos::read_varuint32(buffer);

        for(uint32_t i = 0;i < total_trace; ++i){
            state_history::transaction_trace trace;
            abieos::bin_to_native(trace, buffer);
            handle(trace);
        }
    }


    void handle(const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace){
        m_plugin.applied_action(m_position.value(),sig_block, trace, action_trace);
    }

    void handle(const state_history::transaction_trace& trace){
        if(!m_this_block){
            ilog("this is wrong, trace without block.");
            return;
        }

        state_history::transaction_trace_v0 trx_trace = std::get<state_history::transaction_trace_v0>(trace);


        // ilog("${tid}",("tid",std::string(trx_trace.id)));

        

        for(auto& atrace: trx_trace.action_traces){
            handle(m_this_block.value(), trace, atrace);
        }

        //process failed deffred transaction.
        for(auto& fd_trace: trx_trace.failed_dtrx_trace){
            handle(fd_trace.recurse);
        }


    }


    void process_deltas(abieos::input_buffer buffer){
        auto     num     = abieos::read_varuint32(buffer);
        for (uint32_t i = 0; i < num; ++i) {
            uint32_t version = abieos::read_varuint32(buffer); // ANCHOR: this is a trap, ship side pack data with struct table_delta but here we don't have that struct....
            if(version != 0)continue; //now we use this do a simple version check.

            state_history::table_delta_v0 table_delta;
            bin_to_native(table_delta, buffer);
            m_plugin.applied_delta(m_position.value(), table_delta);
        }
    }


    void handle(const state_history::get_blocks_result_v0& result){
        if (!result.this_block)return;
        
        if(result.this_block.value().block_num <= m_latest_block){
            //emit fork signal if we got a smaller block
            m_plugin.signal_fork(m_position.value());
        }
        m_latest_block = result.this_block.value().block_num;
        m_position.emplace(result.this_block.value());
        
        if(result.block){
            process_block(result.block.value());
        }

        if(result.traces){
            process_traces(result.traces.value());
        }

        if(result.deltas){
            process_deltas(result.deltas.value());
        }

        if(m_this_block){
            m_plugin.block_finish(m_position.value(),result.last_irreversible);
            m_position.reset();
            m_this_block.reset();
        }

    }

    void handle(const state_history::get_status_result_v0& result){
        if(m_status.has_value())m_status.reset();
        m_status.emplace(result);
    }

    void init(){
        applied_state_connection.emplace(
            appbase::app().find_plugin<state_history_plugin>()->applied_status.connect(
                [&](const state_history::get_status_result_v0& status){
                    handle(status);
                }
            )
        );

        applied_block_connection.emplace(
            appbase::app().find_plugin<state_history_plugin>()->applied_blocks.connect(
                [&](const state_history::get_blocks_result_v0& result){
                    handle(result);
                }
            )
        );

    }


};




static appbase::abstract_plugin& _parser_plugin = appbase::app().register_plugin<parser_plugin>();

parser_plugin::parser_plugin():my(std::make_shared<parser_plugin_impl>(*this)){
}

parser_plugin::~parser_plugin(){}


void parser_plugin::set_program_options(appbase::options_description& cli, appbase::options_description& cfg) {
    auto op = cfg.add_options();
}


void parser_plugin::plugin_initialize(const appbase::variables_map& options) {
    my->init();
}
void parser_plugin::plugin_startup() {
}
void parser_plugin::plugin_shutdown() {
}
