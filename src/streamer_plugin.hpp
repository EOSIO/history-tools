// copyright defined in LICENSE.txt

#pragma once
#include <appbase/application.hpp>
#include <abieos.hpp>


struct stream_wrapper_v0 {
   abieos::name      route;
   std::vector<char> data;
};
ABIEOS_REFLECT(stream_wrapper_v0){
    ABIEOS_MEMBER(stream_wrapper_v0, route)
    ABIEOS_MEMBER(stream_wrapper_v0, data)
}


using stream_wrapper = std::variant<stream_wrapper_v0>;

class streamer_plugin : public appbase::plugin<streamer_plugin> {

 public:
   APPBASE_PLUGIN_REQUIRES()
   streamer_plugin();
   virtual ~streamer_plugin();

   virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
   void         plugin_initialize(const appbase::variables_map& options);
   void         plugin_startup();
   void         plugin_shutdown();
   void         stream_data(const char* data, uint64_t data_size);

 private:
   std::shared_ptr<struct streamer_plugin_impl> my;

   void publish_to_streams(const stream_wrapper_v0& sw);
};


