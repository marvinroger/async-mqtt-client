#include "Outpacket.hpp"

using AsyncMqttClientInternals::Outpacket;

uint16_t Outpacket::_packetId = 0;

uint16_t Outpacket::_getNextPacketId() {
  if (++_packetId == 0) {
    ++_packetId;
  }
  return _packetId;
}
