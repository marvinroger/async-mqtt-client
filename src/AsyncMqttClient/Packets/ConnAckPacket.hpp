#pragma once

#include "Arduino.h"
#include "Packet.hpp"
#include "../ParsingInformation.hpp"
#include "../Callbacks.hpp"

namespace AsyncMqttClientInternals {
class ConnAckPacket : public Packet {
 public:
  explicit ConnAckPacket(ParsingInformation* parsingInformation, OnConnAckCallback callback);
  ~ConnAckPacket();

  void parseVariableHeader(char* data, size_t* currentBytePosition);
  void parsePayload(char* data, size_t* currentBytePosition);

 private:
  ParsingInformation* _parsingInformation;
  OnConnAckCallback _callback;

  uint8_t _bytePosition;
  bool _sessionPresent;
  uint8_t _connectReturnCode;
};
}  // namespace AsyncMqttClientInternals
