#include "PublishPacket.hpp"

using AsyncMqttClientInternals::PublishPacket;

PublishPacket::PublishPacket(ParsingInformation* parsingInformation, OnMessageInternalCallback dataCallback, OnPublishInternalCallback completeCallback)
: _parsingInformation(parsingInformation)
, _dataCallback(dataCallback)
, _completeCallback(completeCallback)
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
}

void PublishPacket::parseVariableHeader(char* data, size_t len, size_t* currentBytePosition) {
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
  if (payloadLength == 0) {
    _parsingInformation->bufferState = BufferState::NONE;
    _dataCallback(_topic, nullptr, _qos, 0, 0, 0, _packetId);
    _completeCallback(_packetId, _qos);
  } else {
    _parsingInformation->bufferState = BufferState::PAYLOAD;
  }
}

void PublishPacket::parsePayload(char* data, size_t len, size_t* currentBytePosition) {
  size_t remainToRead = len - (*currentBytePosition);
  if (_payloadBytesRead + remainToRead > _payloadLength) remainToRead = _payloadLength - _payloadBytesRead;

  _dataCallback(_topic, data + (*currentBytePosition), _qos, remainToRead, _payloadBytesRead, _payloadLength, _packetId);
  _payloadBytesRead += remainToRead;
  (*currentBytePosition) += remainToRead;

  if (_payloadBytesRead == _payloadLength) {
    _parsingInformation->bufferState = BufferState::NONE;
    _completeCallback(_packetId, _qos);
  }
}
