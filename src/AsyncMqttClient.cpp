#include "AsyncMqttClient.hpp"

AsyncMqttClient::AsyncMqttClient()
: _client(nullptr)
, _firstPacket(nullptr)
, _lastPacket(nullptr)
, _sendHead(nullptr)
, _sent(0)
, _acked(0)
, _state(DISCONNECTED)
, _tlsBadFingerprint(false)
, _lastClientActivity(0)
, _lastServerActivity(0)
, _lastPingRequestTime(0)
, _generatedClientId{0}
, _ip()
, _host(nullptr)
, _useIp(false)
#if ASYNC_TCP_SSL_ENABLED
, _secure(false)
#endif
, _port(0)
, _keepAlive(15)
, _cleanSession(true)
, _clientId(nullptr)
, _username(nullptr)
, _password(nullptr)
, _willTopic(nullptr)
, _willPayload(nullptr)
, _willPayloadLength(0)
, _willQos(0)
, _willRetain(false)
#if ASYNC_TCP_SSL_ENABLED
, _secureServerFingerprints()
#endif
, _onConnectUserCallbacks()
, _onDisconnectUserCallbacks()
, _onSubscribeUserCallbacks()
, _onUnsubscribeUserCallbacks()
, _onMessageUserCallbacks()
, _onPublishUserCallbacks()
, _parsingInformation { .bufferState = AsyncMqttClientInternals::BufferState::NONE }
, _currentParsedPacket(nullptr)
, _remainingLengthBufferPosition(0)
, _remainingLengthBuffer{0}
, _pendingPubRels() {
#ifdef ESP32
  sprintf(_generatedClientId, "esp32-%06llx", ESP.getEfuseMac());
  _xSemaphore = xSemaphoreCreateMutex();
#elif defined(ESP8266)
  sprintf(_generatedClientId, "esp8266-%06x", ESP.getChipId());
#endif
  _clientId = _generatedClientId;

  setMaxTopicLength(128);
}

AsyncMqttClient::~AsyncMqttClient() {
  delete _currentParsedPacket;
  delete[] _parsingInformation.topicBuffer;
  _clear();
  _pendingPubRels.clear();
  _pendingPubRels.shrink_to_fit();
  _clearQueue(false);  // _clear() doesn't clear session data
#ifdef ESP32
  vSemaphoreDelete(_xSemaphore);
#endif
}

AsyncMqttClient& AsyncMqttClient::setKeepAlive(uint16_t keepAlive) {
  _keepAlive = keepAlive;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::setClientId(const char* clientId) {
  _clientId = clientId;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::setCleanSession(bool cleanSession) {
  _cleanSession = cleanSession;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::setMaxTopicLength(uint16_t maxTopicLength) {
  _parsingInformation.maxTopicLength = maxTopicLength;
  delete[] _parsingInformation.topicBuffer;
  _parsingInformation.topicBuffer = new char[maxTopicLength + 1];
  return *this;
}

AsyncMqttClient& AsyncMqttClient::setCredentials(const char* username, const char* password) {
  _username = username;
  _password = password;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::setWill(const char* topic, uint8_t qos, bool retain, const char* payload, size_t length) {
  _willTopic = topic;
  _willQos = qos;
  _willRetain = retain;
  _willPayload = payload;
  _willPayloadLength = length;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::setServer(IPAddress ip, uint16_t port) {
  _useIp = true;
  _ip = ip;
  _port = port;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::setServer(const char* host, uint16_t port) {
  _useIp = false;
  _host = host;
  _port = port;
  return *this;
}

#if ASYNC_TCP_SSL_ENABLED
AsyncMqttClient& AsyncMqttClient::setSecure(bool secure) {
  _secure = secure;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::addServerFingerprint(const uint8_t* fingerprint) {
  std::array<uint8_t, SHA1_SIZE> newFingerprint;
  memcpy(newFingerprint.data(), fingerprint, SHA1_SIZE);
  _secureServerFingerprints.push_back(newFingerprint);
  return *this;
}
#endif

AsyncMqttClient& AsyncMqttClient::onConnect(AsyncMqttClientInternals::OnConnectUserCallback callback) {
  _onConnectUserCallbacks.push_back(callback);
  return *this;
}

AsyncMqttClient& AsyncMqttClient::onDisconnect(AsyncMqttClientInternals::OnDisconnectUserCallback callback) {
  _onDisconnectUserCallbacks.push_back(callback);
  return *this;
}

AsyncMqttClient& AsyncMqttClient::onSubscribe(AsyncMqttClientInternals::OnSubscribeUserCallback callback) {
  _onSubscribeUserCallbacks.push_back(callback);
  return *this;
}

AsyncMqttClient& AsyncMqttClient::onUnsubscribe(AsyncMqttClientInternals::OnUnsubscribeUserCallback callback) {
  _onUnsubscribeUserCallbacks.push_back(callback);
  return *this;
}

AsyncMqttClient& AsyncMqttClient::onMessage(AsyncMqttClientInternals::OnMessageUserCallback callback) {
  _onMessageUserCallbacks.push_back(callback);
  return *this;
}

AsyncMqttClient& AsyncMqttClient::onPublish(AsyncMqttClientInternals::OnPublishUserCallback callback) {
  _onPublishUserCallbacks.push_back(callback);
  return *this;
}

void AsyncMqttClient::_freeCurrentParsedPacket() {
  delete _currentParsedPacket;
  _currentParsedPacket = nullptr;
}

void AsyncMqttClient::_clear() {
  _lastPingRequestTime = 0;
  _tlsBadFingerprint = false;
  _freeCurrentParsedPacket();
  delete _client;
  _client = nullptr;
  _clearQueue(true);  // keep session data for now

  _parsingInformation.bufferState = AsyncMqttClientInternals::BufferState::NONE;
}

/* TCP */
void AsyncMqttClient::_onConnect() {
  log_i("TCP conn, MQTT CONNECT");
#if ASYNC_TCP_SSL_ENABLED
  if (_secure && _secureServerFingerprints.size() > 0) {
    SSL* clientSsl = _client->getSSL();

    bool sslFoundFingerprint = false;
    for (std::array<uint8_t, SHA1_SIZE> fingerprint : _secureServerFingerprints) {
      if (ssl_match_fingerprint(clientSsl, fingerprint.data()) == SSL_OK) {
        sslFoundFingerprint = true;
        break;
      }
    }

    if (!sslFoundFingerprint) {
      _tlsBadFingerprint = true;
      _client->close(true);
      return;
    }
  }
#endif
  AsyncMqttClientInternals::OutPacket* msg =
  new AsyncMqttClientInternals::ConnectOutPacket(_cleanSession,
                                                 _username,
                                                 _password,
                                                 _willTopic,
                                                 _willRetain,
                                                 _willQos,
                                                 _willPayload,
                                                 _willPayloadLength,
                                                 _keepAlive,
                                                 _clientId);
  _addFront(msg);
  _handleQueue();
}

void AsyncMqttClient::_onDisconnect() {
  log_i("TCP disconn");
  _state = DISCONNECTED;
  AsyncMqttClientDisconnectReason reason;

  if (_tlsBadFingerprint) {
    reason = AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT;
  } else {
    reason = AsyncMqttClientDisconnectReason::TCP_DISCONNECTED;
  }

  _clear();

  for (auto callback : _onDisconnectUserCallbacks) callback(reason);
}

/*
void AsyncMqttClient::_onError(int8_t error) {
  (void)error;
  // _onDisconnect called anyway
}

void AsyncMqttClient::_onTimeout() {
  // disconnection will be handled by ping/pong management
}
*/

void AsyncMqttClient::_onAck(size_t len) {
#if ASYNC_TCP_SSL_ENABLED
  log_i("ack tls %u (%u)", len, _acked);
#else
  _acked += len;
  log_i("ack %u (%u)", len, _acked);
#endif
  _handleQueue();
}

void AsyncMqttClient::_onData(char* data, size_t len) {
  log_i("data rcv (%u)", len);
  size_t currentBytePosition = 0;
  char currentByte;
  _lastServerActivity = millis();
  do {
    switch (_parsingInformation.bufferState) {
      case AsyncMqttClientInternals::BufferState::NONE:
        currentByte = data[currentBytePosition++];
        _parsingInformation.packetType = currentByte >> 4;
        _parsingInformation.packetFlags = (currentByte << 4) >> 4;
        _parsingInformation.bufferState = AsyncMqttClientInternals::BufferState::REMAINING_LENGTH;
        switch (_parsingInformation.packetType) {
          case AsyncMqttClientInternals::PacketType.CONNACK:
            log_i("rcv CONNACK");
            _currentParsedPacket = new AsyncMqttClientInternals::ConnAckPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onConnAck, this, std::placeholders::_1, std::placeholders::_2));
            break;
          case AsyncMqttClientInternals::PacketType.PINGRESP:
            log_i("rcv PINGRESP");
            _currentParsedPacket = new AsyncMqttClientInternals::PingRespPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPingResp, this));
            break;
          case AsyncMqttClientInternals::PacketType.SUBACK:
            log_i("rcv SUBACK");
            _currentParsedPacket = new AsyncMqttClientInternals::SubAckPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onSubAck, this, std::placeholders::_1, std::placeholders::_2));
            break;
          case AsyncMqttClientInternals::PacketType.UNSUBACK:
            log_i("rcv UNSUBACK");
            _currentParsedPacket = new AsyncMqttClientInternals::UnsubAckPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onUnsubAck, this, std::placeholders::_1));
            break;
          case AsyncMqttClientInternals::PacketType.PUBLISH:
            log_i("rcv PUBLISH");
            _currentParsedPacket = new AsyncMqttClientInternals::PublishPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7, std::placeholders::_8, std::placeholders::_9), std::bind(&AsyncMqttClient::_onPublish, this, std::placeholders::_1, std::placeholders::_2));
            break;
          case AsyncMqttClientInternals::PacketType.PUBREL:
            log_i("rcv PUBREL");
            _currentParsedPacket = new AsyncMqttClientInternals::PubRelPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPubRel, this, std::placeholders::_1));
            break;
          case AsyncMqttClientInternals::PacketType.PUBACK:
            log_i("rcv PUBACK");
            _currentParsedPacket = new AsyncMqttClientInternals::PubAckPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPubAck, this, std::placeholders::_1));
            break;
          case AsyncMqttClientInternals::PacketType.PUBREC:
            log_i("rcv PUBREC");
            _currentParsedPacket = new AsyncMqttClientInternals::PubRecPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPubRec, this, std::placeholders::_1));
            break;
          case AsyncMqttClientInternals::PacketType.PUBCOMP:
            log_i("rcv PUBCOMP");
            _currentParsedPacket = new AsyncMqttClientInternals::PubCompPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPubComp, this, std::placeholders::_1));
            break;
          default:
            log_i("rcv PROTOCOL VIOLATION");
            disconnect(true);
            break;
        }
        break;
      case AsyncMqttClientInternals::BufferState::REMAINING_LENGTH:
        currentByte = data[currentBytePosition++];
        _remainingLengthBuffer[_remainingLengthBufferPosition++] = currentByte;
        if (currentByte >> 7 == 0) {
          _parsingInformation.remainingLength = AsyncMqttClientInternals::Helpers::decodeRemainingLength(_remainingLengthBuffer);
          _remainingLengthBufferPosition = 0;
          if (_parsingInformation.remainingLength > 0) {
            _parsingInformation.bufferState = AsyncMqttClientInternals::BufferState::VARIABLE_HEADER;
          } else {
            // PINGRESP is a special case where it has no variable header, so the packet ends right here
            _parsingInformation.bufferState = AsyncMqttClientInternals::BufferState::NONE;
            _onPingResp();
          }
        }
        break;
      case AsyncMqttClientInternals::BufferState::VARIABLE_HEADER:
        _currentParsedPacket->parseVariableHeader(data, len, &currentBytePosition);
        break;
      case AsyncMqttClientInternals::BufferState::PAYLOAD:
        _currentParsedPacket->parsePayload(data, len, &currentBytePosition);
        break;
      default:
        currentBytePosition = len;
    }
  } while (currentBytePosition != len);
}

void AsyncMqttClient::_onPoll() {
  // if there is too much time the client has sent a ping request without a response, disconnect client to avoid half open connections
  if (_lastPingRequestTime != 0 && (millis() - _lastPingRequestTime) >= (_keepAlive * 1000 * 2)) {
    log_w("PING t/o, disconnecting");
    disconnect(true);
    return;
  }
  // send ping to ensure the server will receive at least one message inside keepalive window
  if (_state == CONNECTED && _lastPingRequestTime == 0 && (millis() - _lastClientActivity) >= (_keepAlive * 1000 * 0.7)) {
    _sendPing();

  // send ping to verify if the server is still there (ensure this is not a half connection)
  } else if (_state == CONNECTED && _lastPingRequestTime == 0 && (millis() - _lastServerActivity) >= (_keepAlive * 1000 * 0.7)) {
    _sendPing();
  }
  _handleQueue();
}

/* QUEUE */

void AsyncMqttClient::_insert(AsyncMqttClientInternals::OutPacket* packet) {
  // We only use this for QoS2 PUBREL so there must be a PUBLISH packet present.
  // The queue therefore cannot be empty and _sendHead points to this PUBLISH packet.
  SEMAPHORE_TAKE();
  log_i("new insert #%u", packet->packetType());
  packet->setNext(_sendHead->getNext());
  _sendHead->setNext(packet);
  if (_sendHead == _lastPacket) {  // PUB packet is the only one in the queue
    _lastPacket = packet;
  }
  SEMAPHORE_GIVE();
  _handleQueue();
}

void AsyncMqttClient::_addFront(AsyncMqttClientInternals::OutPacket* packet) {
  // This is only used for the CONNECT packet, to be able to establish a connection
  // before anything else. The queue can be empty or has packets from the continued session.
  // IN both cases, _sendHead should always point to the CONNECT packet afterwards.
  SEMAPHORE_TAKE();
  log_i("new front #%u", packet->packetType());
  if (_firstPacket == nullptr) {
    _lastPacket = packet;
  } else {
    packet->setNext(_firstPacket);
  }
  _firstPacket = packet;
  _sendHead = _firstPacket;
  SEMAPHORE_GIVE();
  _handleQueue();
}

void AsyncMqttClient::_addBack(AsyncMqttClientInternals::OutPacket* packet) {
  SEMAPHORE_TAKE();
  log_i("new back #%u", packet->packetType());
  if (!_lastPacket) {
    _firstPacket = packet;
  } else {
    _lastPacket->setNext(packet);
  }
  _lastPacket = packet;
  if (!_sendHead) _sendHead = _lastPacket;
  SEMAPHORE_GIVE();
  _handleQueue();
}

void AsyncMqttClient::_handleQueue() {
  SEMAPHORE_TAKE();
  // On ESP32, onDisconnect is called within the close()-call. So we need to make sure we don't lock
  bool disconnect = false;

#if ASYNC_TCP_SSL_ENABLED
  while (_sendHead && _client->space() > 500) {
#else
  while (_sendHead && _client->space() > 10) {  // send at least 10 bytes
#endif
    // Try to send first
    if (_sendHead->size() > _sent) {
      #if ASYNC_TCP_SSL_ENABLED
      // On SSL the TCP library returns the total amount of bytes, not just the unencrypted payload length
      // This total amount will also be acked later and it is then impossible to know the size of the unencrypted
      // payload length. So we can not use our own bookkeeping but rely on the TCP library. We therefore
      // copy the data to TCP and immediately "ack" our own data.
      size_t willSend = std::min(_sendHead->size() - _sent, _client->space());
      size_t tlsSent = _client->add(reinterpret_cast<const char*>(_sendHead->data(_sent)), willSend, ASYNC_WRITE_FLAG_COPY);
      _sent += willSend;
      _acked += willSend;
      (void)tlsSent;
      log_i("snd #%u: (tls: %u) %u/%u", _sendHead->packetType(), tlsSent, _sent, _sendHead->size());
      #else
      _sent += _client->add(reinterpret_cast<const char*>(_sendHead->data(_sent)), _sendHead->size() - _sent, 0x00);
      log_i("snd #%u: %u/%u", _sendHead->packetType(), _sent, _sendHead->size());
      #endif
      _client->send();
      _lastClientActivity = millis();
      _lastPingRequestTime = 0;
      if (_sendHead->packetType() == AsyncMqttClientInternals::PacketType.DISCONNECT) {
        disconnect = true;
      }
    }

    // Delete obsolete packets
    while (_firstPacket && _firstPacket->released() && _acked >= _firstPacket->size()) {
      log_i("rmv #%u", _firstPacket->packetType());
      AsyncMqttClientInternals::OutPacket* next = _firstPacket->getNext();
      _acked -= _firstPacket->size();
      if (_firstPacket == _sendHead) {
        _sendHead = next;
        _sent = 0;
      }
      delete _firstPacket;
      _firstPacket = next;
      if (!next) {
        log_i("q empty");
        _lastPacket = next;  // queue is empty
      }
    }

    // Stop processing when we have to wait for an MQTT acknowledgment
    if (_sendHead && _sendHead->size() == _sent) {
      if (_sendHead->released()) {
        log_i("p #%d rel", _sendHead->packetType());
        _sendHead = _sendHead->getNext();
        _sent = 0;
      } else {
        break;  // sending is complete however send next only after mqtt confirmation
      }
    }
  }

  SEMAPHORE_GIVE();
  if (disconnect) {
    log_i("snd DISCONN, disconnecting");
    _client->close();
  }
}

void AsyncMqttClient::_clearQueue(bool keepSessionData) {
  SEMAPHORE_TAKE();
  AsyncMqttClientInternals::OutPacket* current = _firstPacket;
  AsyncMqttClientInternals::OutPacket* currentSend = _sendHead;
  _firstPacket = nullptr;
  _lastPacket = nullptr;
  _sendHead = nullptr;
  bool setDup = true;

  while (current) {
    /* MQTT spec 3.1.2.4 Clean Session:
     *  - QoS 1 and QoS 2 messages which have been sent to the Server, but have not been completely acknowledged.
     *  - QoS 2 messages which have been received from the Server, but have not been completely acknowledged. 
     */
    if (keepSessionData &&
        ((current->packetType() == AsyncMqttClientInternals::PacketType.PUBLISH && current->qos() > 0) ||
         current->packetType() == AsyncMqttClientInternals::PacketType.PUBREC ||
         current->packetType() == AsyncMqttClientInternals::PacketType.PUBREL)) {
      log_i("keep #%u", current->packetType());
      if (setDup && current->packetType() == AsyncMqttClientInternals::PacketType.PUBLISH) {
        reinterpret_cast<AsyncMqttClientInternals::PublishOutPacket*>(current)->setDup();
      }
      if (current == currentSend) {
        setDup = false;
      }
      _addBack(current);
      _lastPacket->setNext(nullptr);
      current = current->getNext();
    } else {
      log_i("rmv #%u", current->packetType());
      AsyncMqttClientInternals::OutPacket* next = current->getNext();
      delete current;
      current = next;
    }
  }
  _sent = 0;
  _acked = 0;
  SEMAPHORE_GIVE();
}

/* MQTT */
void AsyncMqttClient::_onPingResp() {
  log_i("PINGRESP");
  _freeCurrentParsedPacket();
  _lastPingRequestTime = 0;
}

void AsyncMqttClient::_onConnAck(bool sessionPresent, uint8_t connectReturnCode) {
  log_i("CONNACK");
  _freeCurrentParsedPacket();

  SEMAPHORE_TAKE();
  if (_firstPacket && _firstPacket->packetType() == AsyncMqttClientInternals::PacketType.CONNECT) {
    _firstPacket->release();
    log_i("CONNECT released");
  }
  SEMAPHORE_GIVE();

  if (!sessionPresent) {
    _pendingPubRels.clear();
    _pendingPubRels.shrink_to_fit();
    _clearQueue(false);  // remove session data
  }

  if (connectReturnCode == 0) {
    _state = CONNECTED;
    for (auto callback : _onConnectUserCallbacks) callback(sessionPresent);
  } else {
    // Callbacks are handled by the ondisconnect function which is called from the AsyncTcp lib
  }
  _handleQueue();  // send any remaining data from continued session
}

void AsyncMqttClient::_onSubAck(uint16_t packetId, char status) {
  log_i("SUBACK");
  _freeCurrentParsedPacket();
  SEMAPHORE_TAKE();
  if (_sendHead && _sendHead->packetId() == packetId) {
    _sendHead->release();
    log_i("SUB released");
  }
  SEMAPHORE_GIVE();

  for (auto callback : _onSubscribeUserCallbacks) callback(packetId, status);

  _handleQueue();  // subscribe confirmed, ready to send next queued item
}

void AsyncMqttClient::_onUnsubAck(uint16_t packetId) {
  log_i("UNSUBACK");
  _freeCurrentParsedPacket();
  SEMAPHORE_TAKE();
  if (_sendHead && _sendHead->packetId() == packetId) {
    _sendHead->release();
    log_i("UNSUB released");
  }
  SEMAPHORE_GIVE();

  for (auto callback : _onUnsubscribeUserCallbacks) callback(packetId);

  _handleQueue();  // unsubscribe confirmed, ready to send next queued item
}

void AsyncMqttClient::_onMessage(char* topic, char* payload, uint8_t qos, bool dup, bool retain, size_t len, size_t index, size_t total, uint16_t packetId) {
  bool notifyPublish = true;

  if (qos == 2) {
    for (AsyncMqttClientInternals::PendingPubRel pendingPubRel : _pendingPubRels) {
      if (pendingPubRel.packetId == packetId) {
        notifyPublish = false;
        break;
      }
    }
  }

  if (notifyPublish) {
    AsyncMqttClientMessageProperties properties;
    properties.qos = qos;
    properties.dup = dup;
    properties.retain = retain;

    for (auto callback : _onMessageUserCallbacks) callback(topic, payload, properties, len, index, total);
  }
}

void AsyncMqttClient::_onPublish(uint16_t packetId, uint8_t qos) {
  AsyncMqttClientInternals::PendingAck pendingAck;

  if (qos == 1) {
    pendingAck.packetType = AsyncMqttClientInternals::PacketType.PUBACK;
    pendingAck.headerFlag = AsyncMqttClientInternals::HeaderFlag.PUBACK_RESERVED;
    pendingAck.packetId = packetId;
    AsyncMqttClientInternals::OutPacket* msg = new AsyncMqttClientInternals::PubAckOutPacket(pendingAck);
    _addBack(msg);
  } else if (qos == 2) {
    pendingAck.packetType = AsyncMqttClientInternals::PacketType.PUBREC;
    pendingAck.headerFlag = AsyncMqttClientInternals::HeaderFlag.PUBREC_RESERVED;
    pendingAck.packetId = packetId;
    AsyncMqttClientInternals::OutPacket* msg = new AsyncMqttClientInternals::PubAckOutPacket(pendingAck);
    _addBack(msg);

    bool pubRelAwaiting = false;
    for (AsyncMqttClientInternals::PendingPubRel pendingPubRel : _pendingPubRels) {
      if (pendingPubRel.packetId == packetId) {
        pubRelAwaiting = true;
        break;
      }
    }

    if (!pubRelAwaiting) {
      AsyncMqttClientInternals::PendingPubRel pendingPubRel;
      pendingPubRel.packetId = packetId;
      _pendingPubRels.push_back(pendingPubRel);
    }
  }

  _freeCurrentParsedPacket();
}

void AsyncMqttClient::_onPubRel(uint16_t packetId) {
  _freeCurrentParsedPacket();

  AsyncMqttClientInternals::PendingAck pendingAck;
  pendingAck.packetType = AsyncMqttClientInternals::PacketType.PUBCOMP;
  pendingAck.headerFlag = AsyncMqttClientInternals::HeaderFlag.PUBCOMP_RESERVED;
  pendingAck.packetId = packetId;
  if (_sendHead && _sendHead->packetId() == packetId) {
    AsyncMqttClientInternals::OutPacket* msg = new AsyncMqttClientInternals::PubAckOutPacket(pendingAck);
    _sendHead->release();
    _insert(msg);
    log_i("PUBREC released");
  }

  for (size_t i = 0; i < _pendingPubRels.size(); i++) {
    if (_pendingPubRels[i].packetId == packetId) {
      _pendingPubRels.erase(_pendingPubRels.begin() + i);
      _pendingPubRels.shrink_to_fit();
    }
  }
}

void AsyncMqttClient::_onPubAck(uint16_t packetId) {
  _freeCurrentParsedPacket();
  if (_sendHead && _sendHead->packetId() == packetId) {
    _sendHead->release();
    log_i("PUB released");
  }

  for (auto callback : _onPublishUserCallbacks) callback(packetId);
}

void AsyncMqttClient::_onPubRec(uint16_t packetId) {
  _freeCurrentParsedPacket();

  // We will only be sending 1 QoS>0 PUB message at a time (to honor message
  // ordering). So no need to store ACKS in a separate container as it will
  // be stored in the outgoing queue until a PUBCOMP comes in.
  AsyncMqttClientInternals::PendingAck pendingAck;
  pendingAck.packetType = AsyncMqttClientInternals::PacketType.PUBREL;
  pendingAck.headerFlag = AsyncMqttClientInternals::HeaderFlag.PUBREL_RESERVED;
  pendingAck.packetId = packetId;
  log_i("snd PUBREL");

  AsyncMqttClientInternals::OutPacket* msg = new AsyncMqttClientInternals::PubAckOutPacket(pendingAck);
  if (_sendHead && _sendHead->packetId() == packetId) {
    _sendHead->release();
    log_i("PUB released");
  }
  _insert(msg);
}

void AsyncMqttClient::_onPubComp(uint16_t packetId) {
  _freeCurrentParsedPacket();

  // _sendHead points to the PUBREL package
  if (_sendHead && _sendHead->packetId() == packetId) {
    _sendHead->release();
    log_i("PUBREL released");
  }

  for (auto callback : _onPublishUserCallbacks) callback(packetId);
}

void AsyncMqttClient::_sendPing() {
  log_i("PING");
  AsyncMqttClientInternals::OutPacket* msg = new AsyncMqttClientInternals::PingReqOutPacket;
  _addBack(msg);
}

bool AsyncMqttClient::connected() const {
  return _state == CONNECTED;
}

void AsyncMqttClient::connect() {
  if (_state != DISCONNECTED || _client) return;
  log_i("CONNECTING");
  _state = CONNECTING;
  _client = new AsyncClient;
  _client->onConnect([](void* obj, AsyncClient* c) { (static_cast<AsyncMqttClient*>(obj))->_onConnect(); }, this);
  _client->onDisconnect([](void* obj, AsyncClient* c) { (static_cast<AsyncMqttClient*>(obj))->_onDisconnect(); }, this);
  // _client->onError([](void* obj, AsyncClient* c, int8_t error) { (static_cast<AsyncMqttClient*>(obj))->_onError(error); }, this);
  // _client->onTimeout([](void* obj, AsyncClient* c, uint32_t time) { (static_cast<AsyncMqttClient*>(obj))->_onTimeout(); }, this);
  _client->onAck([](void* obj, AsyncClient* c, size_t len, uint32_t time) { (static_cast<AsyncMqttClient*>(obj))->_onAck(len); }, this);
  _client->onData([](void* obj, AsyncClient* c, void* data, size_t len) { (static_cast<AsyncMqttClient*>(obj))->_onData(static_cast<char*>(data), len); }, this);
  _client->onPoll([](void* obj, AsyncClient* c) { (static_cast<AsyncMqttClient*>(obj))->_onPoll(); }, this);
  _client->setNoDelay(true);  // send small packets immediately (PINGREQ/DISCONN are only 2 bytes)

#if ASYNC_TCP_SSL_ENABLED
  if (_useIp) {
    _client->connect(_ip, _port, _secure);
  } else {
    _client->connect(_host, _port, _secure);
  }
#else
  if (_useIp) {
    _client->connect(_ip, _port);
  } else {
    _client->connect(_host, _port);
  }
#endif
}

void AsyncMqttClient::disconnect(bool force) {
  log_i("DISCONNECT (f:%d)", force);
  if (force) {
    _state = DISCONNECTED;
    _client->close(true);
  } else {
    _state = DISCONNECTING;
    AsyncMqttClientInternals::OutPacket* msg = new AsyncMqttClientInternals::DisconnOutPacket;
    _addBack(msg);
  }
}

uint16_t AsyncMqttClient::subscribe(const char* topic, uint8_t qos) {
  if (_state != CONNECTED) return 0;
  log_i("SUBSCRIBE");

  AsyncMqttClientInternals::OutPacket* msg = new AsyncMqttClientInternals::SubscribeOutPacket(topic, qos);
  _addBack(msg);
  return msg->packetId();
}

uint16_t AsyncMqttClient::unsubscribe(const char* topic) {
  if (_state != CONNECTED) return 0;
  log_i("UNSUBSCRIBE");

  AsyncMqttClientInternals::OutPacket* msg = new AsyncMqttClientInternals::UnsubscribeOutPacket(topic);
  _addBack(msg);
  return msg->packetId();
}

uint16_t AsyncMqttClient::publish(const char* topic, uint8_t qos, bool retain, const char* payload, size_t length, bool dup, uint16_t message_id) {
  if (_state != CONNECTED || GET_FREE_MEMORY() < MQTT_MIN_FREE_MEMORY) return 0;
  log_i("PUBLISH");

  AsyncMqttClientInternals::OutPacket* msg = new AsyncMqttClientInternals::PublishOutPacket(topic, qos, retain, payload, length);
  _addBack(msg);
  return msg->packetId();
}

const char* AsyncMqttClient::getClientId() const {
  return _clientId;
}
