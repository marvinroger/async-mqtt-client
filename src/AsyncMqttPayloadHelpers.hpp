#pragma once

#include <functional>
#include <assert.h>

#include <Arduino.h>

#include "AsyncMqttClient.hpp"

#ifndef ASYNCMQTTPAYLOADLENGTH
#define ASYNCMQTTPAYLOADLENGTH 1048
#endif

#ifndef ASYNCMQTTPAYMAXLOADLENGTH
#define ASYNCMQTTPAYMAXLOADLENGTH 8192
#endif

class AsyncMqttPayloadHelper;

namespace AsyncMqttPayloadInternals {
  typedef std::function<void(char* topic, uint8_t* payload, AsyncMqttClientMessageProperties properties, size_t len)> OnMessageUserCallback;
  typedef std::function<void(char* topic, uint8_t* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)> OnChunkedMessageUserCallback;
}

class AsyncMqttPayloadHelper {
 public:
  AsyncMqttPayloadHelper();
  AsyncMqttPayloadHelper& onMessage(AsyncMqttPayloadInternals::OnMessageUserCallback callback);
  AsyncMqttPayloadHelper& onChunkedMessage(AsyncMqttPayloadInternals::OnChunkedMessageUserCallback callback);
  AsyncMqttPayloadHelper& maxSize(size_t maxSize);
  void onData(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);

  template <typename T>
  T as() const {
    assert(sizeof(T) <= _size - 1);
    return *reinterpret_cast<T*>(_data);
  }


 protected:
  AsyncMqttPayloadInternals::OnMessageUserCallback _onMessage;
  AsyncMqttPayloadInternals::OnChunkedMessageUserCallback _onChunkedMessage;
  uint8_t* _data;
  size_t _len;
  size_t _size;
  size_t _maxSize;
};

template <>
inline const char* AsyncMqttPayloadHelper::as<const char*>() const {
  _data[_len] = 0;
  return reinterpret_cast<const char*>(_data);
}

template <>
inline char* AsyncMqttPayloadHelper::as<char*>() const {
  _data[_len] = 0;
  return reinterpret_cast<char*>(_data);
}
