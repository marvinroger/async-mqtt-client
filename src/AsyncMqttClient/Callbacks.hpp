#pragma once

#include <functional>

#include "DisconnectReasons.hpp"

namespace AsyncMqttClientInternals {
typedef std::function<void()> OnConnectCallback;
typedef std::function<void(AsyncMqttClientDisconnectReason reason)> OnDisconnectCallback;
typedef std::function<void(uint16_t packetId, uint8_t qos)> OnSubscribeCallback;
typedef std::function<void(uint16_t packetId)> OnUnsubscribeCallback;
typedef std::function<void(const char* topic, const char* payload, uint8_t qos, size_t len, size_t index, size_t total)> OnPublishCallback;
typedef std::function<void(uint16_t packetId)> OnPublishAckCallback;

typedef std::function<void(bool sessionPresent, uint8_t connectReturnCode)> OnConnAckCallback;
typedef std::function<void()> OnPingRespCallback;
typedef std::function<void(uint16_t packetId, char status)> OnSubAckCallback;
typedef std::function<void(uint16_t packetId)> OnUnsubAckCallback;
typedef std::function<void(const char* topic, const char* payload, uint8_t qos, size_t len, size_t index, size_t total, uint16_t packetId)> OnPublishDataInternalCallback;
typedef std::function<void(uint16_t packetId, uint8_t qos)> OnPublishCompleteInternalCallback;
typedef std::function<void(uint16_t packetId)> OnPubRelCallback;
typedef std::function<void(uint16_t packetId)> OnPubAckCallback;
typedef std::function<void(uint16_t packetId)> OnPubRecCallback;
typedef std::function<void(uint16_t packetId)> OnPubCompCallback;
}  // namespace AsyncMqttClientInternals
