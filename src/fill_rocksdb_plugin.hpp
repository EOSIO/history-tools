// copyright defined in LICENSE.txt

#pragma once
#include "fill_plugin.hpp"
#include "rocksdb_plugin.hpp"

class fill_rocksdb_plugin : public appbase::plugin<fill_rocksdb_plugin> {
 public:
   APPBASE_PLUGIN_REQUIRES((fill_plugin)(rocksdb_plugin))

   fill_rocksdb_plugin();
   virtual ~fill_rocksdb_plugin();

   virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
   void         plugin_initialize(const appbase::variables_map& options);
   void         plugin_startup();
   void         plugin_shutdown();

 private:
   std::shared_ptr<struct fill_rocksdb_plugin_impl> my;
};
