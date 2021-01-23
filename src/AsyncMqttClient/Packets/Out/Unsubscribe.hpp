#pragma once

#include <cstring>  // strlen
#include <vector>

#include "Outpacket.hpp"
#include "../../Flags.hpp"
#include "../../Helpers.hpp"
#include "../../Storage.hpp"

namespace AsyncMqttClientInternals {
class UnsubscribeOutPacket : public Outpacket {
 public:
  UnsubscribeOutPacket(const char* topic);
  const uint8_t* data() const;
  size_t size() const;

  uint16_t packetId() const;

 private:
  std::vector<uint8_t> _data;
  uint16_t _packetId;
}; 
}  // namespace AsyncMqttClientInternals