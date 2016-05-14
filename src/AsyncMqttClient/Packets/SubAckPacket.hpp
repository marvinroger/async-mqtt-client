#pragma once

#include "Arduino.h"
#include "Packet.hpp"
#include "../ParsingInformation.hpp"
#include "../Callbacks.hpp"

namespace AsyncMqttClientInternals {
class SubAckPacket : public Packet {
 public:
  explicit SubAckPacket(ParsingInformation* parsingInformation, OnSubAckCallback callback);
  ~SubAckPacket();

  void parseVariableHeader(const char* data, size_t* currentBytePosition);
  void parsePayload(const char* data, size_t* currentBytePosition);

 private:
  ParsingInformation* _parsingInformation;
  OnSubAckCallback _callback;

  uint8_t _bytePosition;
  char _packetIdMsb;
  uint16_t _packetId;
};
}  // namespace AsyncMqttClientInternals
