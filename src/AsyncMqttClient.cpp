#include "AsyncMqttClient.hpp"

AsyncMqttClient::AsyncMqttClient()
: _keepAlive(15)
, _willPayload(nullptr)
, _nextPacketId(1) {
  _client.onConnect([](void* obj, AsyncClient* c) { (static_cast<AsyncMqttClient*>(obj))->_onConnect(c); }, this);
  _client.onDisconnect([](void* obj, AsyncClient* c) { (static_cast<AsyncMqttClient*>(obj))->_onDisconnect(c); }, this);
  _client.onError([](void* obj, AsyncClient* c, int8_t error) { (static_cast<AsyncMqttClient*>(obj))->_onError(c, error); }, this);
  _client.onTimeout([](void* obj, AsyncClient* c, uint32_t time) { (static_cast<AsyncMqttClient*>(obj))->_onTimeout(c, time); }, this);
  _client.onAck([](void* obj, AsyncClient* c, size_t len, uint32_t time) { (static_cast<AsyncMqttClient*>(obj))->_onAck(c, len, time); }, this);
  _client.onData([](void* obj, AsyncClient* c, void* data, size_t len) { (static_cast<AsyncMqttClient*>(obj))->_onData(c, static_cast<char*>(data), len); }, this);

  _parsingInformation.bufferState = AsyncMqttClientInternals::BufferState::NONE;

  char flashChipId[6 + 1];
  snprintf(flashChipId, 6 + 1, "%06x", ESP.getFlashChipId());
  snprintf(_generatedClientId, 13 + 1, "%s%s", "esp8266", flashChipId);
  _clientId = _generatedClientId;

  _onConnectCallback = []() { };
  _onDisconnectCallback = [](AsyncMqttClientDisconnectReason reason) { (void)reason; };
  _onSubscribeCallback = [](uint16_t packetId, uint8_t qos) { (void)packetId; (void)qos; };
  _onUnsubscribeCallback = [](uint16_t packetId) { (void)packetId; };
  _onPublishReceivedCallback = [](const char* topic, const char* payload, size_t len, uint8_t qos) { (void)topic; (void)payload; (void)len; (void)qos; };
  _onPublishAckCallback = [](uint16_t packetId) { (void)packetId; };
}

AsyncMqttClient::~AsyncMqttClient() {
  delete _currentParsedPacket;
}

AsyncMqttClient& AsyncMqttClient::setKeepAlive(uint16_t keepAlive) {
  _keepAlive = keepAlive;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::setClientId(const char* clientId) {
  _clientId = clientId;
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

AsyncMqttClient& AsyncMqttClient::onConnect(AsyncMqttClientInternals::OnConnectCallback callback) {
  _onConnectCallback = callback;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::onDisconnect(AsyncMqttClientInternals::OnDisconnectCallback callback) {
  _onDisconnectCallback = callback;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::onSubscribeAck(AsyncMqttClientInternals::OnSubscribeCallback callback) {
  _onSubscribeCallback = callback;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::onUnsubscribeAck(AsyncMqttClientInternals::OnUnsubscribeCallback callback) {
  _onUnsubscribeCallback = callback;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::onPublish(AsyncMqttClientInternals::OnPublishCallback callback) {
  _onPublishReceivedCallback = callback;
  return *this;
}

AsyncMqttClient& AsyncMqttClient::onPublishAck(AsyncMqttClientInternals::OnPublishAckCallback callback) {
  _onPublishAckCallback = callback;
  return *this;
}

void AsyncMqttClient::_freeCurrentParsedPacket() {
  delete _currentParsedPacket;
  _currentParsedPacket = nullptr;
}

void AsyncMqttClient::_clear() {
  _keepAliveTicker.detach();
  _connected = false;
  _freeCurrentParsedPacket();
  _pendingPubRels.clear();
  _pendingPubRels.shrink_to_fit();

  _nextPacketId = 1;
  _parsingInformation.bufferState = AsyncMqttClientInternals::BufferState::NONE;
}

/* TCP */
void AsyncMqttClient::_onConnect(AsyncClient* client) {
  (void)client;
  char fixedHeader[5];
  fixedHeader[0] = AsyncMqttClientInternals::PacketType.CONNECT;
  fixedHeader[0] = fixedHeader[0] << 4;
  fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.CONNECT_RESERVED;

  uint16_t protocolNameLength = 4;
  char protocolNameLengthBytes[2];
  protocolNameLengthBytes[0] = protocolNameLength >> 8;
  protocolNameLengthBytes[1] = protocolNameLength & 0xFF;

  char protocolLevel[1];
  protocolLevel[0] = 0x04;

  char connectFlags[1];
  connectFlags[0] = AsyncMqttClientInternals::ConnectFlag.CLEAN_SESSION;
  if (_username != nullptr) connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.USERNAME;
  if (_password != nullptr) connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.PASSWORD;
  if (_willTopic != nullptr) {
    connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.WILL;
    if (_willRetain) connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.WILL_RETAIN;
    switch (_willQos) {
      case 0:
        connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.WILL_QOS0;
        break;
      case 1:
        connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.WILL_QOS1;
        break;
      case 2:
        connectFlags[0] |= AsyncMqttClientInternals::ConnectFlag.WILL_QOS2;
        break;
    }
  }

  char keepAliveBytes[2];
  keepAliveBytes[0] = _keepAlive >> 8;
  keepAliveBytes[1] = _keepAlive & 0xFF;

  uint16_t clientIdLength = strlen(_clientId);
  char clientIdLengthBytes[2];
  clientIdLengthBytes[0] = clientIdLength >> 8;
  clientIdLengthBytes[1] = clientIdLength & 0xFF;

  // Optional fields
  uint16_t willTopicLength = 0;
  char willTopicLengthBytes[2];
  uint16_t willPayloadLength = _willPayloadLength;
  char willPayloadLengthBytes[2];
  if (_willTopic != nullptr) {
    willTopicLength = strlen(_willTopic);
    willTopicLengthBytes[0] = willTopicLength >> 8;
    willTopicLengthBytes[1] = willTopicLength & 0xFF;

    if (_willPayload != nullptr && willPayloadLength == 0) willPayloadLength = strlen(_willPayload);

    willPayloadLengthBytes[0] = willPayloadLength >> 8;
    willPayloadLengthBytes[1] = willPayloadLength & 0xFF;
  }

  uint16_t usernameLength = 0;
  char usernameLengthBytes[2];
  if (_username != nullptr) {
    usernameLength = strlen(_username);
    usernameLengthBytes[0] = usernameLength >> 8;
    usernameLengthBytes[1] = usernameLength & 0xFF;
  }

  uint16_t passwordLength = 0;
  char passwordLengthBytes[2];
  if (_password != nullptr) {
    passwordLength = strlen(_password);
    passwordLengthBytes[0] = passwordLength >> 8;
    passwordLengthBytes[1] = passwordLength & 0xFF;
  }

  uint32_t remainingLength = 2 + protocolNameLength + 1 + 1 + 2 + 2 + clientIdLength;  // always present
  if (_willTopic != nullptr) remainingLength += 2 + willTopicLength + 2 + willPayloadLength;
  if (_username != nullptr) remainingLength += 2 + usernameLength;
  if (_password != nullptr) remainingLength += 2 + passwordLength;
  uint8_t remainingLengthLength = AsyncMqttClientInternals::Helpers::encodeRemainingLength(remainingLength, fixedHeader + 1);
  _client.add(fixedHeader, 1 + remainingLengthLength);
  _client.add(protocolNameLengthBytes, 2);
  _client.add("MQTT", protocolNameLength);
  _client.add(protocolLevel, 1);
  _client.add(connectFlags, 1);
  _client.add(keepAliveBytes, 2);
  _client.add(clientIdLengthBytes, 2);
  _client.add(_clientId, clientIdLength);
  if (_willTopic != nullptr) {
    _client.add(willTopicLengthBytes, 2);
    _client.add(_willTopic, willTopicLength);

    _client.add(willPayloadLengthBytes, 2);
    if (_willPayload != nullptr) _client.add(_willPayload, willPayloadLength);
  }
  if (_username != nullptr) {
    _client.add(usernameLengthBytes, 2);
    _client.add(_username, usernameLength);
  }
  if (_password != nullptr) {
    _client.add(passwordLengthBytes, 2);
    _client.add(_password, passwordLength);
  }
  _client.send();
}

void AsyncMqttClient::_onDisconnect(AsyncClient* client) {
  (void)client;
  _clear();
  _onDisconnectCallback(AsyncMqttClientDisconnectReason::TCP_DISCONNECTED);
}

void AsyncMqttClient::_onError(AsyncClient* client, int8_t error) {
  (void)client;
  (void)error;
  // _onDisconnect called anyway
}

void AsyncMqttClient::_onTimeout(AsyncClient* client, uint32_t time) {
  (void)client;
  (void)time;
  // _onDisconnect called anyway
}

void AsyncMqttClient::_onAck(AsyncClient* client, size_t len, uint32_t time) {
  (void)client;
  (void)len;
  (void)time;
  _keepAliveTicker.detach();
  _keepAliveTicker.once(_keepAlive, _keepAliveTick, &_client);
}

void AsyncMqttClient::_onData(AsyncClient* client, const char* data, size_t len) {
  (void)client;
  size_t currentBytePosition = 0;
  char currentByte;
  do {
    switch (_parsingInformation.bufferState) {
      case AsyncMqttClientInternals::BufferState::NONE:
        currentByte = data[currentBytePosition++];
        _parsingInformation.packetType = currentByte >> 4;
        _parsingInformation.packetFlags = (currentByte << 4) >> 4;
        _parsingInformation.bufferState = AsyncMqttClientInternals::BufferState::REMAINING_LENGTH;
        switch (_parsingInformation.packetType) {
          case AsyncMqttClientInternals::PacketType.CONNACK:
            _currentParsedPacket = new AsyncMqttClientInternals::ConnAckPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onConnAck, this, std::placeholders::_1, std::placeholders::_2));
            break;
          case AsyncMqttClientInternals::PacketType.PINGRESP:
            _currentParsedPacket = new AsyncMqttClientInternals::PingRespPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPingResp, this));
            break;
          case AsyncMqttClientInternals::PacketType.SUBACK:
            _currentParsedPacket = new AsyncMqttClientInternals::SubAckPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onSubAck, this, std::placeholders::_1, std::placeholders::_2));
            break;
          case AsyncMqttClientInternals::PacketType.UNSUBACK:
            _currentParsedPacket = new AsyncMqttClientInternals::UnsubAckPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onUnsubAck, this, std::placeholders::_1));
            break;
          case AsyncMqttClientInternals::PacketType.PUBLISH:
            _currentParsedPacket = new AsyncMqttClientInternals::PublishPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPublish, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
            break;
          case AsyncMqttClientInternals::PacketType.PUBREL:
            _currentParsedPacket = new AsyncMqttClientInternals::PubRelPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPubRel, this, std::placeholders::_1));
            break;
          case AsyncMqttClientInternals::PacketType.PUBACK:
            _currentParsedPacket = new AsyncMqttClientInternals::PubAckPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPubAck, this, std::placeholders::_1));
            break;
          case AsyncMqttClientInternals::PacketType.PUBREC:
            _currentParsedPacket = new AsyncMqttClientInternals::PubRecPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPubRec, this, std::placeholders::_1));
            break;
          case AsyncMqttClientInternals::PacketType.PUBCOMP:
            _currentParsedPacket = new AsyncMqttClientInternals::PubCompPacket(&_parsingInformation, std::bind(&AsyncMqttClient::_onPubComp, this, std::placeholders::_1));
            break;
          default:
            break;
        }
        break;
      case AsyncMqttClientInternals::BufferState::REMAINING_LENGTH:
        currentByte = data[currentBytePosition++];
        _remainingLengthBuffer[_remainingLengthBufferPosition++] = currentByte;
        if (currentByte >> 7 == 0) {
          _parsingInformation.remainingLength = AsyncMqttClientInternals::Helpers::decodeRemainingLength(_remainingLengthBuffer);
          _remainingLengthBufferPosition = 0;
          _parsingInformation.bufferState = AsyncMqttClientInternals::BufferState::VARIABLE_HEADER;
        }
        break;
      case AsyncMqttClientInternals::BufferState::VARIABLE_HEADER:
        _currentParsedPacket->parseVariableHeader(data, &currentBytePosition);
        break;
      case AsyncMqttClientInternals::BufferState::PAYLOAD:
        _currentParsedPacket->parsePayload(data, &currentBytePosition);
        break;
      default:
        currentBytePosition = len;
    }
  } while (currentBytePosition != len);
}

/* MQTT */
void AsyncMqttClient::_onPingResp() {
  _freeCurrentParsedPacket();
}

void AsyncMqttClient::_onConnAck(bool sessionPresent, uint8_t connectReturnCode) {
  (void)sessionPresent;
  _freeCurrentParsedPacket();

  if (connectReturnCode == 0) {
    _connected = true;
    _onConnectCallback();
  } else {
    _clear();
    _onDisconnectCallback(static_cast<AsyncMqttClientDisconnectReason>(connectReturnCode));
  }
}

void AsyncMqttClient::_onSubAck(uint16_t packetId, char status) {
  _freeCurrentParsedPacket();

  _onSubscribeCallback(packetId, status);
}

void AsyncMqttClient::_onUnsubAck(uint16_t packetId) {
  _freeCurrentParsedPacket();

  _onUnsubscribeCallback(packetId);
}

void AsyncMqttClient::_onPublish(const char* topic, const char* payload, size_t len, uint8_t qos, uint16_t packetId) {
  bool notifyPublish = true;

  if (qos == 1) {
    char fixedHeader[2];
    fixedHeader[0] = AsyncMqttClientInternals::PacketType.PUBACK;
    fixedHeader[0] = fixedHeader[0] << 4;
    fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.PUBACK_RESERVED;
    fixedHeader[1] = 2;

    char packetIdBytes[2];
    packetIdBytes[0] = packetId >> 8;
    packetIdBytes[1] = packetId & 0xFF;

    _client.add(fixedHeader, 2);
    _client.add(packetIdBytes, 2);
    _client.send();
  } else if (qos == 2) {
    char fixedHeader[2];
    fixedHeader[0] = AsyncMqttClientInternals::PacketType.PUBREC;
    fixedHeader[0] = fixedHeader[0] << 4;
    fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.PUBREC_RESERVED;
    fixedHeader[1] = 2;

    char packetIdBytes[2];
    packetIdBytes[0] = packetId >> 8;
    packetIdBytes[1] = packetId & 0xFF;

    _client.add(fixedHeader, 2);
    _client.add(packetIdBytes, 2);
    _client.send();

    for (size_t i = 0; i < _pendingPubRels.size(); i++) {
      if (_pendingPubRels[i].packetId == packetId) {
        notifyPublish = false;
      }
    }

    if (notifyPublish) {
      AsyncMqttClientInternals::PendingPubRel pendingPubRel;
      pendingPubRel.packetId = packetId;
      _pendingPubRels.push_back(pendingPubRel);
    }
  }

  if (notifyPublish) _onPublishReceivedCallback(topic, payload, len, qos);
  _freeCurrentParsedPacket();
}

void AsyncMqttClient::_onPubRel(uint16_t packetId) {
  _freeCurrentParsedPacket();

  char fixedHeader[2];
  fixedHeader[0] = AsyncMqttClientInternals::PacketType.PUBCOMP;
  fixedHeader[0] = fixedHeader[0] << 4;
  fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.PUBCOMP_RESERVED;
  fixedHeader[1] = 2;

  char packetIdBytes[2];
  packetIdBytes[0] = packetId >> 8;
  packetIdBytes[1] = packetId & 0xFF;

  _client.add(fixedHeader, 2);
  _client.add(packetIdBytes, 2);
  _client.send();

  for (size_t i = 0; i < _pendingPubRels.size(); i++) {
    if (_pendingPubRels[i].packetId == packetId) {
      _pendingPubRels.erase(_pendingPubRels.begin() + i);
      _pendingPubRels.shrink_to_fit();
    }
  }
}

void AsyncMqttClient::_onPubAck(uint16_t packetId) {
  _freeCurrentParsedPacket();

  _onPublishAckCallback(packetId);
}

void AsyncMqttClient::_onPubRec(uint16_t packetId) {
  _freeCurrentParsedPacket();

  char fixedHeader[2];
  fixedHeader[0] = AsyncMqttClientInternals::PacketType.PUBREL;
  fixedHeader[0] = fixedHeader[0] << 4;
  fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.PUBREL_RESERVED;
  fixedHeader[1] = 2;

  char packetIdBytes[2];
  packetIdBytes[0] = packetId >> 8;
  packetIdBytes[1] = packetId & 0xFF;

  _client.add(fixedHeader, 2);
  _client.add(packetIdBytes, 2);
  _client.send();
}

void AsyncMqttClient::_onPubComp(uint16_t packetId) {
  _freeCurrentParsedPacket();
  _onPublishAckCallback(packetId);
}

void AsyncMqttClient::_keepAliveTick(AsyncClient* client) {
  char fixedHeader[2];
  fixedHeader[0] = AsyncMqttClientInternals::PacketType.PINGREQ;
  fixedHeader[0] = fixedHeader[0] << 4;
  fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.PINGREQ_RESERVED;
  fixedHeader[1] = 0;

  client->add(fixedHeader, 2);
  client->send();
}

uint16_t AsyncMqttClient::_getNextPacketId() {
  uint16_t nextPacketId = _nextPacketId;

  if (_nextPacketId == 65535) _nextPacketId = 0;  // 0 is forbidden
  _nextPacketId++;

  return nextPacketId;
}

bool AsyncMqttClient::connected() {
  return _connected;
}

void AsyncMqttClient::connect() {
  if (_useIp) {
    _client.connect(_ip, _port);
  } else {
    _client.connect(_host, _port);
  }
}

void AsyncMqttClient::disconnect() {
  char fixedHeader[2];
  fixedHeader[0] = AsyncMqttClientInternals::PacketType.DISCONNECT;
  fixedHeader[0] = fixedHeader[0] << 4;
  fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.DISCONNECT_RESERVED;
  fixedHeader[1] = 0;

  _client.add(fixedHeader, 2);
  _client.send();
  _client.close();
}

uint16_t AsyncMqttClient::subscribe(const char* topic, uint8_t qos) {
  char fixedHeader[5];
  fixedHeader[0] = AsyncMqttClientInternals::PacketType.SUBSCRIBE;
  fixedHeader[0] = fixedHeader[0] << 4;
  fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.SUBSCRIBE_RESERVED;

  uint16_t packetId = _getNextPacketId();
  char packetIdBytes[2];
  packetIdBytes[0] = packetId >> 8;
  packetIdBytes[1] = packetId & 0xFF;

  uint16_t topicLength = strlen(topic);
  char topicLengthBytes[2];
  topicLengthBytes[0] = topicLength >> 8;
  topicLengthBytes[1] = topicLength & 0xFF;

  char qosByte[1];
  qosByte[0] = qos;

  uint8_t remainingLengthLength = AsyncMqttClientInternals::Helpers::encodeRemainingLength(2 + 2 + topicLength + 1, fixedHeader + 1);
  _client.add(fixedHeader, 1 + remainingLengthLength);
  _client.add(packetIdBytes, 2);
  _client.add(topicLengthBytes, 2);
  _client.add(topic, topicLength);
  _client.add(qosByte, 1);
  _client.send();

  return packetId;
}

uint16_t AsyncMqttClient::unsubscribe(const char* topic) {
  char fixedHeader[5];
  fixedHeader[0] = AsyncMqttClientInternals::PacketType.UNSUBSCRIBE;
  fixedHeader[0] = fixedHeader[0] << 4;
  fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.UNSUBSCRIBE_RESERVED;

  uint16_t packetId = _getNextPacketId();
  char packetIdBytes[2];
  packetIdBytes[0] = packetId >> 8;
  packetIdBytes[1] = packetId & 0xFF;

  uint16_t topicLength = strlen(topic);
  char topicLengthBytes[2];
  topicLengthBytes[0] = topicLength >> 8;
  topicLengthBytes[1] = topicLength & 0xFF;

  uint8_t remainingLengthLength = AsyncMqttClientInternals::Helpers::encodeRemainingLength(2 + 2 + topicLength, fixedHeader + 1);
  _client.add(fixedHeader, 1 + remainingLengthLength);
  _client.add(packetIdBytes, 2);
  _client.add(topicLengthBytes, 2);
  _client.add(topic, topicLength);
  _client.send();

  return packetId;
}

uint16_t AsyncMqttClient::publish(const char* topic, uint8_t qos, bool retain, const char* payload, size_t length) {
  char fixedHeader[5];
  fixedHeader[0] = AsyncMqttClientInternals::PacketType.PUBLISH;
  fixedHeader[0] = fixedHeader[0] << 4;
  fixedHeader[0] = fixedHeader[0] | AsyncMqttClientInternals::HeaderFlag.CONNECT_RESERVED;
  if (retain) fixedHeader[0] |= AsyncMqttClientInternals::HeaderFlag.PUBLISH_RETAIN;
  switch (qos) {
    case 0:
      fixedHeader[0] |= AsyncMqttClientInternals::HeaderFlag.PUBLISH_QOS0;
      break;
    case 1:
      fixedHeader[0] |= AsyncMqttClientInternals::HeaderFlag.PUBLISH_QOS1;
      break;
    case 2:
      fixedHeader[0] |= AsyncMqttClientInternals::HeaderFlag.PUBLISH_QOS2;
      break;
  }

  uint16_t topicLength = strlen(topic);
  char topicLengthBytes[2];
  topicLengthBytes[0] = topicLength >> 8;
  topicLengthBytes[1] = topicLength & 0xFF;

  uint16_t packetId = 0;
  char packetIdBytes[2];
  if (qos != 0) {
    packetId = _getNextPacketId();
    packetIdBytes[0] = packetId >> 8;
    packetIdBytes[1] = packetId & 0xFF;
  }

  uint32_t payloadLength = length;
  if (payload != nullptr && payloadLength == 0) payloadLength = strlen(payload);

  uint32_t remainingLength = 2 + topicLength + payloadLength;
  if (qos != 0) remainingLength += 2;
  uint8_t remainingLengthLength = AsyncMqttClientInternals::Helpers::encodeRemainingLength(remainingLength, fixedHeader + 1);
  _client.add(fixedHeader, 1 + remainingLengthLength);
  _client.add(topicLengthBytes, 2);
  _client.add(topic, topicLength);
  if (qos != 0) _client.add(packetIdBytes, 2);
  if (payload != nullptr) _client.add(payload, payloadLength);
  _client.send();

  return packetId;
}
