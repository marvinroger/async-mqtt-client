#pragma once

#include "OutPacket.hpp"
#include "../../Flags.hpp"
#include "../../Helpers.hpp"
#include "../../Storage.hpp"

namespace AsyncMqttClientInternals {
class PubAckOutPacket : public OutPacket {
 public:
  explicit PubAckOutPacket(PendingAck pendingAck);
  const uint8_t* data(size_t index = 0) const;
  size_t size() const;

  bool released() const override;
  uint16_t packetId() const override;

 private:
  uint8_t _data[4];
  uint16_t _packetId;
};
}  // namespace AsyncMqttClientInternals
