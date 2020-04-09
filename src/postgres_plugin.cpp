// copyright defined in LICENSE.txt
#include "postgres_plugin.hpp"
#include "parser_plugin.hpp"
#include <fc/log/logger.hpp>



struct sql{

std::string table_name;
std::vector< std::tuple<std::string,std::string>> data;


sql& operator()(const std::string& _table_name){
    table_name = _table_name;
    return *this;
}

sql& operator()(const std::tuple<std::string,std::string>& tp){
    data.push_back(tp);
    return *this;
}

sql& operator()(const std::string& field, const std::string& value){
    data.emplace_back(field,value);
    return *this;
}

std::string operator()(){
    std::stringstream ss;
    ss << "INSERT INTO " << table_name << " ( ";
    for(uint32_t i = 0; i < data.size(); ++i){
        if(i!=0)ss << ",";
        ss << std::get<0>(data[i]);
    }
    ss<< " ) VALUES ( ";
    for(uint32_t i = 0; i < data.size(); ++i){
        if(i!=0)ss << ",";
        ss << std::get<1>(data[i]);
    }
    ss<< " )";
    return ss.str();
}

};


struct table_builder{
    virtual void handle(const state_history::block_position& pos,const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace) = 0;
    virtual ~table_builder(){}
};


struct action_trace_builder: table_builder{


    void handle(const state_history::block_position& pos,const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace) override final{
        state_history::transaction_trace_v0 trace_v0 = std::get<state_history::transaction_trace_v0>(trace);
        sql query;
        query("action_trace")
             ("block_num",std::to_string(pos.block_num))
             ("timestamp",std::string(sig_block.timestamp))
             ("transaction_id",std::string(trace_v0.id))
             ("transaction_status",state_history::to_string(trace_v0.status));
        
        std::cout << query() << std::endl;
    }

};



struct postgres_plugin_impl: std::enable_shared_from_this<postgres_plugin_impl> {
    std::optional<bsg::scoped_connection>     applied_action_connection;
    std::vector<std::unique_ptr<table_builder>> table_builders;    
    //datas
    postgres_plugin& m_plugin;

    postgres_plugin_impl(postgres_plugin& plugin):m_plugin(plugin){}

    void handle(const state_history::block_position& pos,const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace){
        for(auto& tb: table_builders){
            tb->handle(pos,sig_block,trace,action_trace);
        }
    }


    void init(){
        applied_action_connection.emplace(
            appbase::app().find_plugin<parser_plugin>()->applied_action.connect(
                [&](const state_history::block_position& pos, const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace){
                    handle(pos,sig_block,trace,action_trace);
                }
            )
        );

        auto up = std::make_unique<action_trace_builder>();
        table_builders.push_back(std::move(up));
    }


};




static appbase::abstract_plugin& _postgres_plugin = appbase::app().register_plugin<postgres_plugin>();

postgres_plugin::postgres_plugin():my(std::make_shared<postgres_plugin_impl>(*this)){
}

postgres_plugin::~postgres_plugin(){}


void postgres_plugin::set_program_options(appbase::options_description& cli, appbase::options_description& cfg) {
    auto op = cfg.add_options();
}


void postgres_plugin::plugin_initialize(const appbase::variables_map& options) {
    my->init();
}
void postgres_plugin::plugin_startup() {
}
void postgres_plugin::plugin_shutdown() {
}