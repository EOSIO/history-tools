#pragma once

#include "stream.hpp"
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <fc/log/logger.hpp>

class rabbitmq : public stream_handler {
   AmqpClient::Channel::ptr_t channel_;
   std::string                name_;

 public:
   rabbitmq(std::string host, int port, std::string user, std::string password, std::string name) {
      name_    = name;
      ilog("connecting AMQP...");
      channel_ = AmqpClient::Channel::Create(host, port, user, password);
      ilog("AMQP connected");
      channel_->DeclareQueue(name, false, true, false, false);
   }

   void publish(const char* data, uint64_t data_size) {
      std::string message(data, data_size);
      auto amqp_message = AmqpClient::BasicMessage::Create(message);
      channel_->BasicPublish("", name_, amqp_message);
   }
};
