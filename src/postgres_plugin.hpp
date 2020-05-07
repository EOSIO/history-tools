// copyright defined in LICENSE.txt

#pragma once
#include "state_history.hpp"
#include <appbase/application.hpp>
#include <boost/signals2/signal.hpp>
#include "postgres_utils.hpp"

namespace bsg = boost::signals2;



struct table_builder{
    std::string name;

    const std::string& get_name(){return name;}
    virtual SQL::insert handle(const state_history::block_position& pos,const state_history::signed_block& sig_block, const state_history::transaction_trace& trace, const state_history::action_trace& action_trace){return SQL::insert();}
    virtual std::vector<std::string> create(){return std::vector<std::string>();}
    virtual std::vector<std::string> drop(){return std::vector<std::string>();}
    virtual std::vector<std::string> truncate(const state_history::block_position& pos){return std::vector<std::string>();}
    virtual void endofblock(const state_history::block_position& pos, const state_history::block_position& lib_pos){};

    virtual ~table_builder(){}
};



class postgres_plugin : public appbase::plugin<postgres_plugin> {
  public:
    APPBASE_PLUGIN_REQUIRES()

    postgres_plugin();
    virtual ~postgres_plugin();

    void     set_program_options(appbase::options_description& cli, appbase::options_description& cfg);
    void     plugin_initialize(const appbase::variables_map& options);
    void     plugin_startup();
    void     plugin_shutdown();
    
    std::shared_ptr<struct postgres_plugin_impl> my;
};
