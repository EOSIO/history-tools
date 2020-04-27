#pragma once

#include "stream.hpp"
#include <fc/log/logger.hpp>

class logger : public stream_handler {
 public:
   logger() {}

   void publish(const char* data, uint64_t data_size) {
      ilog("logger stream [${data_size}] >> ${data}", ("data", data)("data_size", data_size));
   }
};
