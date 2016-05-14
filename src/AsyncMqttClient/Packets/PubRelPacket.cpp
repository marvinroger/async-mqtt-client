#include "PubRelPacket.hpp"

using AsyncMqttClientInternals::PubRelPacket;

PubRelPacket::PubRelPacket(ParsingInformation* parsingInformation, OnPubRelCallback callback)
: _parsingInformation(parsingInformation)
, _callback(callback)
, _bytePosition(0)
, _packetIdMsb(0)
, _packetId(0) {
}

PubRelPacket::~PubRelPacket() {
}

void PubRelPacket::parseVariableHeader(const char* data, size_t* currentBytePosition) {
  char currentByte = data[(*currentBytePosition)++];
  if (_bytePosition++ == 0) {
    _packetIdMsb = currentByte;
  } else {
    _packetId = currentByte | _packetIdMsb << 8;
    _parsingInformation->bufferState = BufferState::NONE;
    _callback(_packetId);
  }
}

void PubRelPacket::parsePayload(const char* data, size_t* currentBytePosition) {
  (void)data;
  (void)currentBytePosition;
}
