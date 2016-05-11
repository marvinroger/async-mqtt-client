#pragma once

#include "Arduino.h"
#include "Packet.hpp"
#include "../ParsingInformation.hpp"

namespace AsyncMqttClientInternals {
class PingRespPacket : public Packet {
 public:
  explicit PingRespPacket(ParsingInformation* parsingInformation);
  ~PingRespPacket();

  void parseVariableHeader(char* data, size_t* currentBytePosition);
  void parsePayload(char* data, size_t* currentBytePosition);

 private:
  ParsingInformation* _parsingInformation;
};
}  // namespace AsyncMqttClientInternals
