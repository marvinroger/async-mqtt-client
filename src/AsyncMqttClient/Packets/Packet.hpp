#pragma once

namespace AsyncMqttClientInternals {
class Packet {
 public:
  Packet() {}
  virtual ~Packet() {}

  virtual void parseVariableHeader(char* data, size_t* currentBytePosition) {}
  virtual void parsePayload(char* data, size_t* currentBytePosition) {}
};
}  // namespace AsyncMqttClientInternals
