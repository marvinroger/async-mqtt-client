#pragma once

#include <stdint.h>  // uint*_t
#include <stddef.h>  // size_t

namespace AsyncMqttClientInternals {
class Outpacket {
 public:
  ~Outpacket() {}
  virtual const uint8_t* data() const = 0;
  virtual size_t size() const = 0;

 protected:
  static uint16_t _getNextPacketId();

 private:
  static uint16_t _packetId;
}; 
}  // namespace AsyncMqttClientInternals