#pragma once

#include "Arduino.h"
#include "Packet.hpp"
#include "../ParsingInformation.hpp"
#include "../Callbacks.hpp"

namespace AsyncMqttClientInternals {
class PubAckPacket : public Packet {
 public:
  explicit PubAckPacket(ParsingInformation* parsingInformation, OnPubAckCallback callback);
  ~PubAckPacket();

  void parseVariableHeader(char* data, size_t* currentBytePosition);
  void parsePayload(char* data, size_t* currentBytePosition);

 private:
  ParsingInformation* _parsingInformation;
  OnPubAckCallback _callback;

  uint8_t _bytePosition;
  char _packetIdMsb;
  uint16_t _packetId;
};
}  // namespace AsyncMqttClientInternals
