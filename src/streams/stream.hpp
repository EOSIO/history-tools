#pragma once

class stream_handler {
 public:
   virtual ~stream_handler() {}
   virtual void publish(const char* data, uint64_t data_size) = 0;
};
