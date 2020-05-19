#pragma once
#include <abieos.hpp>
#include <fc/log/logger.hpp>


class stream_handler {
 public:
   virtual ~stream_handler() {}
   virtual const std::vector<abieos::name>& get_routes() const = 0;
   virtual void publish(const char* data, uint64_t data_size) = 0;

   bool check_route(const abieos::name& stream_route) {
      if (get_routes().size() == 0) {
         return true;
      }

      for (const auto& name : get_routes()) {
         if (name == stream_route) {
            return true;
         }
      }

      return false;
   }
};

inline std::vector<abieos::name> extract_routings(const std::string& routings_str) {
   std::vector<abieos::name> routing_keys{};
   bool star = false;
   std::string routings = routings_str;
   while (routings.size() > 0) {
      size_t      pos          = routings.find(",");
      size_t      route_length = pos == std::string::npos ? routings.length() : pos;
      std::string route        = routings.substr(0, pos);
      ilog("extracting route ${route}", ("route", route));
      if (route != "*") {
         routing_keys.emplace_back(abieos::name(route.c_str()));
      } else {
         star = true;
      }
      routings.erase(0, route_length + 1);
   }
   if (star && !routing_keys.empty()) {
      throw std::runtime_error(std::string("Invalid routings '") + routings_str + "'");
   }
   return routing_keys;
}

