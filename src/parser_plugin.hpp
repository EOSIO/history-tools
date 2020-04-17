// copyright defined in LICENSE.txt

#pragma once
#include "state_history.hpp"
#include <appbase/application.hpp>
#include <boost/signals2/signal.hpp>

namespace bsg = boost::signals2;


class parser_plugin : public appbase::plugin<parser_plugin> {
  public:
    APPBASE_PLUGIN_REQUIRES()

    parser_plugin();
    virtual ~parser_plugin();

    void     set_program_options(appbase::options_description& cli, appbase::options_description& cfg);
    void     plugin_initialize(const appbase::variables_map& options);
    void     plugin_startup();
    void     plugin_shutdown();

    bsg::signal<void(const state_history::block_position& pos, const state_history::signed_block&, const state_history::transaction_trace&, const state_history::action_trace&)> applied_action;
    bsg::signal<void(const state_history::block_position& pos)> block_finish;
    bsg::signal<void(const state_history::block_position& pos)> signal_fork;
    std::shared_ptr<struct parser_plugin_impl> my;
};
