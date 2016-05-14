#include "PublishPacket.hpp"

using AsyncMqttClientInternals::PublishPacket;

PublishPacket::PublishPacket(ParsingInformation* parsingInformation, OnPublishInternalCallback callback)
: _parsingInformation(parsingInformation)
, _callback(callback)
, _dup(false)
, _qos(0)
, _retain(0)
, _bytePosition(0)
, _topicLengthMsb(0)
, _topicLength(0)
, _topic(nullptr)
, _packetIdMsb(0)
, _packetId(0)
, _payloadLength(0)
, _payload(nullptr)
, _payloadBytesRead(0) {
    _dup = _parsingInformation->packetFlags & HeaderFlag.PUBLISH_DUP;
    _retain = _parsingInformation->packetFlags & HeaderFlag.PUBLISH_RETAIN;
    char qosMasked = _parsingInformation->packetFlags & 0x06;
    switch (qosMasked) {
      case HeaderFlag.PUBLISH_QOS0:
        _qos = 0;
        break;
      case HeaderFlag.PUBLISH_QOS1:
        _qos = 1;
        break;
      case HeaderFlag.PUBLISH_QOS2:
        _qos = 2;
        break;
    }
}

PublishPacket::~PublishPacket() {
  delete[] _topic;
  delete[] _payload;
}

void PublishPacket::parseVariableHeader(const char* data, size_t* currentBytePosition) {
  char currentByte = data[(*currentBytePosition)++];
  if (_bytePosition == 0) {
    _topicLengthMsb = currentByte;
  } else if (_bytePosition == 1) {
    _topicLength = currentByte | _topicLengthMsb << 8;
    _topic = new char[_topicLength + 1];
    _topic[_topicLength + 1 - 1] = '\0';
  } else if (_bytePosition >= 2 && _bytePosition < 2 + _topicLength) {
    _topic[_bytePosition - 2] = currentByte;
    if (_bytePosition == 2 + _topicLength - 1 && _qos == 0) {
      _preparePayloadHandling(_parsingInformation->remainingLength - (_bytePosition + 1));
      return;
    }
  } else if (_bytePosition == 2 + _topicLength) {
    _packetIdMsb = currentByte;
  } else {
    _packetId = currentByte | _packetIdMsb << 8;
    _preparePayloadHandling(_parsingInformation->remainingLength - (_bytePosition + 1));
  }
  _bytePosition++;
}

void PublishPacket::_preparePayloadHandling(uint32_t payloadLength) {
  _payloadLength = payloadLength;
  _payload = new char[_payloadLength + 1];
  _payload[_payloadLength + 1 - 1] = '\0';
  if (payloadLength == 0) {
    _parsingInformation->bufferState = BufferState::NONE;
    _callback(_topic, _payload, _payloadLength, _qos, _packetId);
  } else {
    _parsingInformation->bufferState = BufferState::PAYLOAD;
  }
}

void PublishPacket::parsePayload(const char* data, size_t* currentBytePosition) {
  char currentByte = data[(*currentBytePosition)++];
  _payload[_payloadBytesRead++] = currentByte;

  if (_payloadBytesRead == _payloadLength) {
    _parsingInformation->bufferState = BufferState::NONE;
    _callback(_topic, _payload, _payloadLength, _qos, _packetId);
  }
}
