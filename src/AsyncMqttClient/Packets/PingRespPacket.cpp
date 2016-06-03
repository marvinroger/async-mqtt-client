#include "PingRespPacket.hpp"

using AsyncMqttClientInternals::PingRespPacket;

PingRespPacket::PingRespPacket(ParsingInformation* parsingInformation, OnPingRespInternalCallback callback)
: _parsingInformation(parsingInformation)
, _callback(callback) {
}

PingRespPacket::~PingRespPacket() {
}

void PingRespPacket::parseVariableHeader(const char* data, size_t len, size_t* currentBytePosition) {
  (void)data;
  (void)currentBytePosition;
  _parsingInformation->bufferState = BufferState::NONE;
  _callback();
}

void PingRespPacket::parsePayload(const char* data, size_t len, size_t* currentBytePosition) {
  (void)data;
  (void)currentBytePosition;
}
