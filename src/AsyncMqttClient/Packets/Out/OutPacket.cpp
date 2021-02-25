#include "OutPacket.hpp"

using AsyncMqttClientInternals::OutPacket;

OutPacket::OutPacket()
: next(nullptr)
, timeout(0)
, noTries(0)
, _released(true) {}

OutPacket::~OutPacket() {}

bool OutPacket::released() const {
  return _released;
}

uint8_t OutPacket::packetType() const {
  return data(0)[0] >> 4;
}

uint16_t OutPacket::packetId() const {
  return 0;
}

uint8_t OutPacket::qos() const {
  if (packetType() == AsyncMqttClientInternals::PacketType.PUBLISH) {
    return (data()[1] & 0x06) >> 1;
  }
  return 0;
}

void OutPacket::release() {
  _released = true;
}

uint16_t OutPacket::_packetId = 0;

uint16_t OutPacket::_getNextPacketId() {
  if (++_packetId == 0) {
    ++_packetId;
  }
  return _packetId;
}
