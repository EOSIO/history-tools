// copyright defined in LICENSE.txt

#pragma once
#include "state_history.hpp"
#include <appbase/application.hpp>
#include <boost/signals2/signal.hpp>

namespace bsg = boost::signals2;


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
