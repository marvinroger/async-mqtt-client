#include <cassert>
#include <cstdio>

#include "AsyncMqttClient.hpp"

static uint16_t packetId(const std::vector<uint8_t>& packet, size_t offset) {
  return (static_cast<uint16_t>(packet.at(offset)) << 8) | packet.at(offset + 1);
}

int main() {
  AsyncMqttClient mqtt;
  AsyncClient& tcp = *AsyncClient::instance;
  mqtt.setServer("broker", 1883).setClientId("packet-lifetime-test");
  mqtt.connect();
  tcp.completeConnect();
  tcp.receive({0x20, 2, 0, 0});
  assert(mqtt.connected());

  for (unsigned i = 0; i < 32; ++i) {
    assert(mqtt.publish("test", 0, false, "immediate send") == 1);
    assert(tcp.sent.back().at(0) == 0x30);
  }

  const uint16_t publishId = mqtt.publish("test", 1, false, "acknowledged");
  assert(publishId != 0);
  assert(packetId(tcp.sent.back(), 8) == publishId);
  tcp.receive({0x40, 2, static_cast<uint8_t>(publishId >> 8), static_cast<uint8_t>(publishId)});
  tcp.poll();

  const uint16_t subscribeId = mqtt.subscribe("test", 0);
  assert(subscribeId != 0);
  assert(packetId(tcp.sent.back(), 2) == subscribeId);
  tcp.receive({0x90, 3, static_cast<uint8_t>(subscribeId >> 8), static_cast<uint8_t>(subscribeId), 0});

  const uint16_t unsubscribeId = mqtt.unsubscribe("test");
  assert(unsubscribeId != 0);
  assert(packetId(tcp.sent.back(), 2) == unsubscribeId);
  tcp.receive({0xb0, 2, static_cast<uint8_t>(unsubscribeId >> 8), static_cast<uint8_t>(unsubscribeId)});

  assert(mqtt.publish("test", 0, false, "after acknowledgments") == 1);
  std::puts("Packet lifetime regression tests passed");
}
