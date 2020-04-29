// copyright defined in LICENSE.txt

#include "streamer_plugin.hpp"
#include "streams/logger.hpp"
#include "streams/rabbitmq.hpp"
#include "streams/stream.hpp"

#include <fc/exception/exception.hpp>
#include <memory>

using namespace appbase;
using namespace std::literals;

struct streamer_plugin_impl {
   std::vector<std::unique_ptr<stream_handler>> streams;
};

static abstract_plugin& _streamer_plugin = app().register_plugin<streamer_plugin>();

streamer_plugin::streamer_plugin() : my(std::make_shared<streamer_plugin_impl>()) {}

streamer_plugin::~streamer_plugin() {}

void streamer_plugin::set_program_options(options_description& cli, options_description& cfg) {
   auto op = cfg.add_options();
   op("stream-rabbits", bpo::value<std::vector<string>>()->composing(),
      "RabbitMQ Streams if any; Format: USER:PASSWORD@ADDRESS:PORT/QUEUE");
   op("stream-logger", bpo::bool_switch()->default_value(false), "Print Stream to Log");
}

void streamer_plugin::plugin_initialize(const variables_map& options) {
   try {
      if (options.at("stream-logger").as<bool>()) {
         ilog("adding logger stream");
         my->streams.emplace_back(std::make_unique<logger>(logger{}));
      }

      if (options.count("stream-rabbits")) {
         auto rabbits = options.at("stream-rabbits").as<std::vector<std::string>>();
         initialize_rabbits(rabbits);
      }
   }
   FC_LOG_AND_RETHROW()
}

void streamer_plugin::initialize_rabbits(const std::vector<std::string>& rabbits) {
   for (auto rabbit : rabbits) {
      size_t      pos        = rabbit.find_last_of("/");
      std::string queue_name = "stream.default";
      if (pos != std::string::npos) {
         queue_name = rabbit.substr(pos + 1, rabbit.length());
         rabbit.erase(pos, rabbit.length());
      }

      std::string user     = "guest";
      std::string password = "guest";
      pos                  = rabbit.find("@");
      if (pos != std::string::npos) {
         auto auth_pos = rabbit.substr(0, pos).find(":");
         user          = rabbit.substr(0, auth_pos);
         password      = rabbit.substr(auth_pos + 1, pos - auth_pos - 1);
         rabbit.erase(0, pos + 1);
      }

      pos              = rabbit.find(":");
      std::string host = rabbit.substr(0, pos);
      int         port = std::stoi(rabbit.substr(pos + 1, rabbit.length()));

      ilog("adding rabbitmq stream ${h}:${p} -- queue: ${queue} | auth: ${user}/****",
           ("h", host)("p", port)("queue", queue_name)("user", user));
      rabbitmq rmq{ host, port, user, password, queue_name };
      my->streams.emplace_back(std::make_unique<rabbitmq>(rmq));
   }
}

void streamer_plugin::plugin_startup() {
   cloner_plugin* cloner = app().find_plugin<cloner_plugin>();
   if (cloner) {
      cloner->set_streamer([&](const char* data, uint64_t data_size) { stream_data(data, data_size); });
   }
}

void streamer_plugin::plugin_shutdown() {}

void streamer_plugin::stream_data(const char* data, uint64_t data_size) {
   eosio::input_stream bin(data, data_size);
   if (eosio::result<stream_wrapper> res = eosio::from_bin<stream_wrapper>(bin)) {
      auto sw = std::get<stream_wrapper_v0>(res.value());
      for (auto& stream : my->streams) { stream->publish(sw.data.data(), sw.data.size()); }
   } else {
      eosio::check(res.error());
   }
}
