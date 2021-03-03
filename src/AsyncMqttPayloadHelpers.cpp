#include "AsyncMqttPayloadHelpers.hpp"

AsyncMqttPayloadHelper::AsyncMqttPayloadHelper()
: _onMessage(nullptr)
, _onChunkedMessage(nullptr)
, _data(nullptr)
, _len(0)
, _size(ASYNCMQTTPAYLOADLENGTH)
, _maxSize(ASYNCMQTTPAYMAXLOADLENGTH) {
  _data = new uint8_t[ASYNCMQTTPAYLOADLENGTH];
}

AsyncMqttPayloadHelper& AsyncMqttPayloadHelper::onMessage(AsyncMqttPayloadInternals::OnMessageUserCallback callback) {
  _onMessage = callback;
  return *this;
}

AsyncMqttPayloadHelper& AsyncMqttPayloadHelper::onChunkedMessage(AsyncMqttPayloadInternals::OnChunkedMessageUserCallback callback) {
  _onChunkedMessage = callback;
  return *this;
}

AsyncMqttPayloadHelper& AsyncMqttPayloadHelper::maxSize(size_t maxSize) {
  _maxSize = maxSize;
  return *this;
}

void AsyncMqttPayloadHelper::onData(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  // payload is bigger then max: return chunked
  if (total > _maxSize) {
    if (_onChunkedMessage) _onChunkedMessage(topic, reinterpret_cast<uint8_t*>(payload), properties, len, index, total);
    return;
  }

  // start new packet, increase size if neccesary
  if (index == 0) {
    if (total > _size) {
      delete[] _data;
      _size = total + 1;  // +1: give room to 0-terminator in case of c-string conversion
      _data = new uint8_t[_size];
    }
    _len = 0;
  }

  // add data, call callback when we've got everything
  memcpy(&_data[_len], reinterpret_cast<uint8_t*>(payload), len);
  _len += len;
  if (_len == total) {
    if (_onMessage) _onMessage(topic, _data, properties, _len);
  }
}
