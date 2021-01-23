#pragma once

#include "Outpacket.hpp"
#include "../../Flags.hpp"
#include "../../Helpers.hpp"

namespace AsyncMqttClientInternals {
class DisconnOutPacket : public Outpacket {
 public:
  DisconnOutPacket();
  const uint8_t* data() const;
  size_t size() const;

 private:
  uint8_t _data[2];
}; 
}  // namespace AsyncMqttClientInternals