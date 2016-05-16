#pragma once

#include "Arduino.h"
#include "Packet.hpp"
#include "../Flags.hpp"
#include "../ParsingInformation.hpp"
#include "../Callbacks.hpp"

namespace AsyncMqttClientInternals {
class PublishPacket : public Packet {
 public:
  explicit PublishPacket(ParsingInformation* parsingInformation, OnPublishDataInternalCallback dataCallback, OnPublishCompleteInternalCallback completeCallback);
  ~PublishPacket();

  void parseVariableHeader(const char* data, size_t len, size_t* currentBytePosition);
  void parsePayload(const char* data, size_t len, size_t* currentBytePosition);

 private:
  ParsingInformation* _parsingInformation;
  OnPublishDataInternalCallback _dataCallback;
  OnPublishCompleteInternalCallback _completeCallback;

  void _preparePayloadHandling(uint32_t payloadLength);

  bool _dup;
  uint8_t _qos;
  bool _retain;

  uint8_t _bytePosition;
  char _topicLengthMsb;
  uint16_t _topicLength;
  char* _topic;
  char _packetIdMsb;
  uint16_t _packetId;
  uint32_t _payloadLength;
  uint32_t _payloadBytesRead;
};
}  // namespace AsyncMqttClientInternals
