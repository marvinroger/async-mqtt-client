#pragma once

namespace AsyncMqttClientInternals {
class Packet {
 public:
  virtual ~Packet() {}

  virtual void parseVariableHeader(const char* data, size_t* currentBytePosition) = 0;
  virtual void parsePayload(const char* data, size_t* currentBytePosition) = 0;
};
}  // namespace AsyncMqttClientInternals
