#include "PubAck.hpp"

using AsyncMqttClientInternals::PubAckOutPacket;

PubAckOutPacket::PubAckOutPacket(PendingAck pendingAck) {
    _data[0] = pendingAck.packetType;
    _data[0] = _data[0] << 4;
    _data[0] = _data[0] | pendingAck.headerFlag;
    _data[1] = 2;
    _data[2] = pendingAck.packetId >> 8;
    _data[3] = pendingAck.packetId & 0xFF;
    _released = true;
}

const uint8_t* PubAckOutPacket::data(size_t index) const {
  return &_data[index];;
}

size_t PubAckOutPacket::size() const {
  return 2;
}