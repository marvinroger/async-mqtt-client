#pragma once

namespace AsyncMqttClientInternals {
struct PendingSubscription {
  uint16_t packetId;
};

struct PendingPubRel {
  uint16_t packetId;
};

struct PendingPubAck {
  uint16_t packetId;
};

struct PendingPubRec {
  uint16_t packetId;
};

struct PendingPubComp {
  uint16_t packetId;
};
}  // namespace AsyncMqttClientInternals
