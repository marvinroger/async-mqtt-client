#pragma once

#include "Outpacket.hpp"
#include "../../Flags.hpp"
#include "../../Helpers.hpp"
#include "../../Storage.hpp"

namespace AsyncMqttClientInternals {
class PubAckOutPacket : public Outpacket {
 public:
  PubAckOutPacket(PendingAck pendingAck);
  const uint8_t* data() const;
  size_t size() const;

 private:
  uint8_t _data[4];
}; 
}  // namespace AsyncMqttClientInternals