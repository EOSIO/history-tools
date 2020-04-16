// copyright defined in LICENSE.txt

#pragma once
#include "state_history.hpp"
#include <appbase/application.hpp>
#include <boost/signals2/signal.hpp>

namespace bsg = boost::signals2;

class state_history_plugin : public appbase::plugin<state_history_plugin> {
  public:
    APPBASE_PLUGIN_REQUIRES()

    state_history_plugin();
    virtual ~state_history_plugin();

    void     set_program_options(appbase::options_description& cli, appbase::options_description& cfg);
    void     plugin_initialize(const appbase::variables_map& options);
    void     plugin_startup();
    void     plugin_shutdown();

    bsg::signal<void(const state_history::get_status_result_v0&)> applied_status;
    bsg::signal<void(const state_history::get_blocks_result_v0&)> applied_blocks;

    void set_initial_block_num(uint32_t block_num);

    std::shared_ptr<struct state_history_plugin_impl> my;
};
