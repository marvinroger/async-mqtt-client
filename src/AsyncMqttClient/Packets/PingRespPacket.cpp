#include "PingRespPacket.hpp"

using AsyncMqttClientInternals::PingRespPacket;

PingRespPacket::PingRespPacket(ParsingInformation* parsingInformation)
: _parsingInformation(parsingInformation) {
}

PingRespPacket::~PingRespPacket() {
}

void PingRespPacket::parseVariableHeader(char* data, size_t* currentBytePosition) {
  _parsingInformation->bufferState = BufferState::NONE;
}

void PingRespPacket::parsePayload(char* data, size_t* currentBytePosition) {
  // No payload
}
