#include <cassert>
#include <cstdio>

#include "AsyncMqttClient.hpp"

class Connection {
 public:
  AsyncMqttClient mqtt;
  AsyncClient& tcp = *AsyncClient::instance;
  unsigned connections = 0;
  unsigned disconnections = 0;

  explicit Connection(bool synchronousClose) {
    tcp.synchronousClose = synchronousClose;
    mqtt.setKeepAlive(60).setServer("broker", 1883).setClientId("timeout-test");
    mqtt.onConnect([this](bool) { ++connections; });
    mqtt.onDisconnect([this](AsyncMqttClientDisconnectReason reason) {
      assert(reason == AsyncMqttClientDisconnectReason::TCP_DISCONNECTED);
      assert(!mqtt.connected());
      ++disconnections;
    });
  }

  void establish() {
    mqtt.connect();
    const auto firstPacket = tcp.sent.size();
    tcp.completeConnect();
    assert(!mqtt.connected());
    assert(tcp.sent[firstPacket][0] == 0x10);
    tcp.receive({0x20, 2, 0, 0});
    assert(mqtt.connected());
  }

  void timeout() {
    const auto sent = tcp.sent.size();
    const auto closes = tcp.closeCalls;
    tcp.timeout();
    assert(!mqtt.connected());
    assert(tcp.closeCalls == closes + 1);
    assert(tcp.sent.size() == sent);
    if (!tcp.synchronousClose) tcp.completeClose();
    assert(disconnections == tcp.closeCalls);
    tcp.timeout();
    assert(tcp.closeCalls == closes + 1);
  }

  void subscribeAndPublish() {
    const uint16_t id = mqtt.subscribe("test", 0);
    assert(id != 0);
    tcp.receive({0x90, 3, static_cast<uint8_t>(id >> 8), static_cast<uint8_t>(id), 0});
    const uint16_t publishId = mqtt.publish("test", 1, false, "recovered");
    assert(publishId != 0);
    assert(tcp.sent.back()[0] == 0x32);
    tcp.receive({0x40, 2, static_cast<uint8_t>(publishId >> 8), static_cast<uint8_t>(publishId)});
    tcp.poll();
    assert(mqtt.connected());
  }
};

static void testHealthyConnection(bool synchronousClose) {
  Connection connection(synchronousClose);
  connection.establish();
  for (unsigned i = 0; i < 30; ++i) {
    currentMillis += 42000;
    connection.tcp.poll();
    assert(connection.tcp.sent.back()[0] == 0xc0);
    connection.tcp.receive({0xd0, 0});
  }
  connection.subscribeAndPublish();
  assert(connection.connections == 1 && connection.disconnections == 0);
}

static void testSilentConnection(bool synchronousClose, bool publish) {
  Connection connection(synchronousClose);
  connection.establish();
  for (unsigned i = 0; i < 3; ++i) {
    if (publish) {
      assert(connection.mqtt.publish("test", 1, false, "lost") != 0);
    } else {
      currentMillis += 42000;
      connection.tcp.poll();
      assert(connection.tcp.sent.back()[0] == 0xc0);
    }
    connection.timeout();
    connection.establish();
    connection.subscribeAndPublish();
    assert(connection.connections == i + 2);
  }
}

static void testHandshakeTimeout(bool synchronousClose) {
  Connection connection(synchronousClose);
  connection.mqtt.connect();
  connection.tcp.completeConnect();
  connection.timeout();
  connection.establish();
  connection.subscribeAndPublish();
  assert(connection.connections == 1 && connection.tcp.connectCalls == 2);
}

static void testPendingPublication(bool synchronousClose) {
  Connection connection(synchronousClose);
  connection.establish();
  assert(connection.mqtt.publish("test", 1, false, "unacknowledged") != 0);
  assert(connection.mqtt.publish("test", 0, false, "queued") != 0);
  connection.timeout();
  connection.establish();
  connection.subscribeAndPublish();
}

static void testMissingPingResponse(bool synchronousClose) {
  Connection connection(synchronousClose);
  connection.establish();
  currentMillis += 42000;
  connection.tcp.poll();
  assert(connection.tcp.sent.back()[0] == 0xc0);
  currentMillis += 60000;
  assert(connection.mqtt.publish("test", 1, false, "outgoing traffic") != 0);
  connection.tcp.poll();
  assert(connection.mqtt.connected());
  currentMillis += 60000;
  connection.tcp.poll();
  assert(!connection.mqtt.connected());
  assert(connection.tcp.closeCalls == 1);
  if (!synchronousClose) connection.tcp.completeClose();
  assert(connection.disconnections == 1);
  connection.establish();
  connection.subscribeAndPublish();
}

int main() {
  for (bool synchronousClose : {true, false}) {
    testHealthyConnection(synchronousClose);
    testSilentConnection(synchronousClose, false);
    testSilentConnection(synchronousClose, true);
    testHandshakeTimeout(synchronousClose);
    testPendingPublication(synchronousClose);
    testMissingPingResponse(synchronousClose);
  }
  std::puts("All 12 timeout regression scenarios passed");
}
