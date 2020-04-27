// copyright defined in LICENSE.txt

#include "streamer_plugin.hpp"

#include <fc/exception/exception.hpp>

using namespace appbase;
using namespace std::literals;

struct streamer_plugin_impl {
   std::optional<uint32_t> test_num = {};
};

static abstract_plugin& _streamer_plugin = app().register_plugin<streamer_plugin>();

streamer_plugin::streamer_plugin() : my(std::make_shared<streamer_plugin_impl>()) {}

streamer_plugin::~streamer_plugin() {}

void streamer_plugin::set_program_options(options_description& cli, options_description& cfg) {
   auto op = cfg.add_options();
}

void streamer_plugin::plugin_initialize(const variables_map& options) {
   try {
      my->test_num = 99;
   }
   FC_LOG_AND_RETHROW()
}

void streamer_plugin::plugin_startup() {}

void streamer_plugin::plugin_shutdown() {}

void streamer_plugin::stream_data(const char* data, uint64_t data_size) {
   ilog("streaming [${s}] >>> ${c}\n\n", ("s", data_size)("c", data));
}
