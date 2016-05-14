#include "PubCompPacket.hpp"

using AsyncMqttClientInternals::PubCompPacket;

PubCompPacket::PubCompPacket(ParsingInformation* parsingInformation, OnPubCompCallback callback)
: _parsingInformation(parsingInformation)
, _callback(callback)
, _bytePosition(0)
, _packetIdMsb(0)
, _packetId(0) {
}

PubCompPacket::~PubCompPacket() {
}

void PubCompPacket::parseVariableHeader(const char* data, size_t* currentBytePosition) {
  char currentByte = data[(*currentBytePosition)++];
  if (_bytePosition++ == 0) {
    _packetIdMsb = currentByte;
  } else {
    _packetId = currentByte | _packetIdMsb << 8;
    _parsingInformation->bufferState = BufferState::NONE;
    _callback(_packetId);
  }
}

void PubCompPacket::parsePayload(const char* data, size_t* currentBytePosition) {
  (void)data;
  (void)currentBytePosition;
}
