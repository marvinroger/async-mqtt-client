#pragma once

#include <functional>
#include <utility>
#include <vector>

#include "Arduino.h"

#define ASYNC_WRITE_FLAG_COPY 1

class AsyncClient {
 public:
  using ConnectHandler = std::function<void(void*, AsyncClient*)>;
  using AckHandler = std::function<void(void*, AsyncClient*, size_t, uint32_t)>;
  using DataHandler = std::function<void(void*, AsyncClient*, void*, size_t)>;
  using TimeoutHandler = std::function<void(void*, AsyncClient*, uint32_t)>;

  static inline AsyncClient* instance;
  bool synchronousClose = true;
  unsigned connectCalls = 0;
  unsigned closeCalls = 0;
  std::vector<std::vector<uint8_t>> sent;

  AsyncClient() { instance = this; }

  void onConnect(ConnectHandler cb, void* arg) { _connect = cb; _connectArg = arg; }
  void onDisconnect(ConnectHandler cb, void* arg) { _disconnect = cb; _disconnectArg = arg; }
  void onPoll(ConnectHandler cb, void* arg) { _poll = cb; _pollArg = arg; }
  void onAck(AckHandler, void*) {}
  void onData(DataHandler cb, void* arg) { _data = cb; _dataArg = arg; }
  void onTimeout(TimeoutHandler cb, void* arg) { _timeout = cb; _timeoutArg = arg; }
  void setNoDelay(bool) {}
  void setRxTimeout(uint32_t) {}

  bool connect(IPAddress, uint16_t) { return connect("broker", 1883); }
  bool connect(const char*, uint16_t) { ++connectCalls; return true; }

  void completeConnect() {
    _open = true;
    _connect(_connectArg, this);
  }

  void close(bool = false) {
    ++closeCalls;
    _open = false;
    if (synchronousClose) completeClose();
  }

  void completeClose() { _disconnect(_disconnectArg, this); }

  size_t space() const { return _open ? 4096 : 0; }
  size_t add(const char* data, size_t size, uint8_t) {
    _outgoing.insert(_outgoing.end(), data, data + size);
    return size;
  }

  bool send() {
    sent.push_back(std::move(_outgoing));
    _outgoing.clear();
    return true;
  }

  void receive(std::vector<uint8_t> bytes) { _data(_dataArg, this, bytes.data(), bytes.size()); }
  void poll() { _poll(_pollArg, this); }
  void timeout(uint32_t elapsed = 5000) {
    if (_timeout) _timeout(_timeoutArg, this, elapsed);
  }

 private:
  bool _open = false;
  std::vector<uint8_t> _outgoing;
  ConnectHandler _connect, _disconnect, _poll;
  DataHandler _data;
  TimeoutHandler _timeout;
  void *_connectArg = nullptr, *_disconnectArg = nullptr, *_pollArg = nullptr;
  void *_dataArg = nullptr, *_timeoutArg = nullptr;
};
