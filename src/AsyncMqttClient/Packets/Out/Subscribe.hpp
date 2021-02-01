#pragma once

#include <cstring>  // strlen
#include <vector>

#include "OutPacket.hpp"
#include "../../Flags.hpp"
#include "../../Helpers.hpp"
#include "../../Storage.hpp"

namespace AsyncMqttClientInternals {
class SubscribeOutPacket : public OutPacket {
 public:
  SubscribeOutPacket(const char* topic, uint8_t qos);
  const uint8_t* data(size_t index = 0) const;
  size_t size() const;

  uint16_t packetId() const;

 private:
  std::vector<uint8_t> _data;
  uint16_t _packetId;
}; 
}  // namespace AsyncMqttClientInternals