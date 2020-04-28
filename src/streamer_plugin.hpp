// copyright defined in LICENSE.txt

#pragma once
#include <appbase/application.hpp>
#include "cloner_plugin.hpp"

class streamer_plugin : public appbase::plugin<streamer_plugin> {
 public:
   APPBASE_PLUGIN_REQUIRES((cloner_plugin))

   streamer_plugin();
   virtual ~streamer_plugin();

   virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
   void         plugin_initialize(const appbase::variables_map& options);
   void         plugin_startup();
   void         plugin_shutdown();
   void         stream_data(const char* data, uint64_t data_size);

 private:
   std::shared_ptr<struct streamer_plugin_impl> my;

   void initialize_rabbits(const std::vector<std::string>& rabbits);
};
