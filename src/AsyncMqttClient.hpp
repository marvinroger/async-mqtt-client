#pragma once

#include <functional>
#include <vector>

#include "Arduino.h"

#include <ESPAsyncTCP.h>

#include "AsyncMqttClient/Flags.hpp"
#include "AsyncMqttClient/ParsingInformation.hpp"
#include "AsyncMqttClient/Helpers.hpp"
#include "AsyncMqttClient/Callbacks.hpp"
#include "AsyncMqttClient/DisconnectReasons.hpp"
#include "AsyncMqttClient/Storage.hpp"

#include "AsyncMqttClient/Packets/Packet.hpp"
#include "AsyncMqttClient/Packets/ConnAckPacket.hpp"
#include "AsyncMqttClient/Packets/PingRespPacket.hpp"
#include "AsyncMqttClient/Packets/SubAckPacket.hpp"
#include "AsyncMqttClient/Packets/UnsubAckPacket.hpp"
#include "AsyncMqttClient/Packets/PublishPacket.hpp"
#include "AsyncMqttClient/Packets/PubRelPacket.hpp"
#include "AsyncMqttClient/Packets/PubAckPacket.hpp"
#include "AsyncMqttClient/Packets/PubRecPacket.hpp"
#include "AsyncMqttClient/Packets/PubCompPacket.hpp"

class AsyncMqttClient {
 public:
  AsyncMqttClient();
  ~AsyncMqttClient();

  AsyncMqttClient& setKeepAlive(uint16_t keepAlive);
  AsyncMqttClient& setClientId(const char* clientId);
  AsyncMqttClient& setCredentials(const char* username, const char* password = nullptr);
  AsyncMqttClient& setWill(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0);
  AsyncMqttClient& setServer(IPAddress ip, uint16_t port);
  AsyncMqttClient& setServer(const char* host, uint16_t port);

  AsyncMqttClient& onConnect(AsyncMqttClientInternals::OnConnectCallback callback);
  AsyncMqttClient& onDisconnect(AsyncMqttClientInternals::OnDisconnectCallback callback);
  AsyncMqttClient& onSubscribeAck(AsyncMqttClientInternals::OnSubscribeCallback callback);
  AsyncMqttClient& onUnsubscribeAck(AsyncMqttClientInternals::OnUnsubscribeCallback callback);
  AsyncMqttClient& onPublish(AsyncMqttClientInternals::OnPublishCallback callback);
  AsyncMqttClient& onPublishAck(AsyncMqttClientInternals::OnPublishAckCallback callback);

  bool connected();
  void connect();
  void disconnect();
  uint16_t subscribe(const char* topic, uint8_t qos);
  uint16_t unsubscribe(const char* topic);
  uint16_t publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0);

 private:
  AsyncClient _client;

  bool _connected;
  uint32_t _lastActivity;

  char _generatedClientId[13 + 1];  // esp8266abc123
  IPAddress _ip;
  const char* _host;
  bool _useIp;
  uint16_t _port;
  uint16_t _keepAlive;
  const char* _clientId;
  const char* _username;
  const char* _password;
  const char* _willTopic;
  const char* _willPayload;
  uint16_t _willPayloadLength;
  uint8_t _willQos;
  bool _willRetain;

  AsyncMqttClientInternals::OnConnectCallback _onConnectCallback;
  AsyncMqttClientInternals::OnDisconnectCallback _onDisconnectCallback;
  AsyncMqttClientInternals::OnSubscribeCallback _onSubscribeCallback;
  AsyncMqttClientInternals::OnUnsubscribeCallback _onUnsubscribeCallback;
  AsyncMqttClientInternals::OnPublishCallback _onPublishReceivedCallback;
  AsyncMqttClientInternals::OnPublishAckCallback _onPublishAckCallback;

  AsyncMqttClientInternals::ParsingInformation _parsingInformation;
  AsyncMqttClientInternals::Packet* _currentParsedPacket;
  uint8_t _remainingLengthBufferPosition;
  char _remainingLengthBuffer[4];

  uint16_t _nextPacketId;

  std::vector<AsyncMqttClientInternals::PendingPubRel> _pendingPubRels;

  void _clear();
  void _freeCurrentParsedPacket();

  // TCP
  void _onConnect(AsyncClient* client);
  void _onDisconnect(AsyncClient* client);
  void _onError(AsyncClient* client, int8_t error);
  void _onTimeout(AsyncClient* client, uint32_t time);
  void _onAck(AsyncClient* client, size_t len, uint32_t time);
  void _onData(AsyncClient* client, const char* data, size_t len);
  void _onPoll(AsyncClient* client);

  // MQTT
  void _onPingResp();
  void _onConnAck(bool sessionPresent, uint8_t connectReturnCode);
  void _onSubAck(uint16_t packetId, char status);
  void _onUnsubAck(uint16_t packetId);
  void _onPublishData(const char* topic, const char* payload, uint8_t qos, size_t len, size_t index, size_t total, uint16_t packetId);
  void _onPublishComplete(uint16_t packetId, uint8_t qos);
  void _onPubRel(uint16_t packetId);
  void _onPubAck(uint16_t packetId);
  void _onPubRec(uint16_t packetId);
  void _onPubComp(uint16_t packetId);

  void _sendPing();

  uint16_t _getNextPacketId();
};
