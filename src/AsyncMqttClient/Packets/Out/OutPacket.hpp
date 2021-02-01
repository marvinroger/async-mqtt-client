#pragma once

#include <stdint.h>  // uint*_t
#include <stddef.h>  // size_t
#include <algorithm>  // std::min

#include "../../Flags.hpp"

namespace AsyncMqttClientInternals {
class OutPacket {
 public:
  OutPacket();
  virtual ~OutPacket();
  virtual const uint8_t* data(size_t index = 0) const = 0;
  virtual size_t size() const = 0;
  bool released() const;
  uint8_t packetType() const;
  virtual uint16_t packetId() const;
  uint8_t qos() const;
  void release();

  OutPacket* getNext() const;
  void setNext(OutPacket* packet);

 protected:
  static uint16_t _getNextPacketId();
  OutPacket* _next;
  bool _released;

 private:
  static uint16_t _packetId;
}; 
}  // namespace AsyncMqttClientInternals