#pragma once

#include "Arduino.h"
#include "Packet.hpp"
#include "../ParsingInformation.hpp"
#include "../Callbacks.hpp"

namespace AsyncMqttClientInternals {
class PingRespPacket : public Packet {
 public:
  explicit PingRespPacket(ParsingInformation* parsingInformation, OnPingRespCallback callback);
  ~PingRespPacket();

  void parseVariableHeader(const char* data, size_t* currentBytePosition);
  void parsePayload(const char* data, size_t* currentBytePosition);

 private:
  ParsingInformation* _parsingInformation;
  OnPingRespCallback _callback;
};
}  // namespace AsyncMqttClientInternals
