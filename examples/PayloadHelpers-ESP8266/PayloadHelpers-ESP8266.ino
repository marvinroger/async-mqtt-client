#include <Arduino.h>
#include <AsyncMqttClient.h>
#include <AsyncMqttPayloadHelpers.h>

AsyncMqttPayloadHelper payloadHelper;
AsyncMqttClient mqttClient;

void onMessage(char* topic, uint8_t* payload, AsyncMqttClientMessageProperties properties, size_t len) {
  Serial.print("new message:\n");
  Serial.print("  payload HEX: ");
  for (size_t i = 0; i < len; ++i) {
    Serial.printf("%02x ", payload[i]);
  }
  Serial.print("\n");
  Serial.print("  payload cstring: ");
  Serial.printf("%s\n", payloadHelper.as<const char*>());
  Serial.print("  payload int32_t: ");
  Serial.printf("%i\n", payloadHelper.as<int32_t>());
}

void onChunkedMessage(char* topic, uint8_t* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.print("new big message:\n");
  Serial.printf(" rcv %u/%u\n", len + index, total);
}

void setup() {
  Serial.begin(74880);
  delay(5000);
  Serial.println();
  Serial.println();

  // setup AsyncMqttPayloadHelper
  payloadHelper.onMessage(onMessage)
               .onChunkedMessage(onChunkedMessage)
               .maxSize(20);

  // static sample data
  AsyncMqttClientMessageProperties properties;
  properties.qos = 0;
  properties.dup = false;
  properties.retain = false;
  char topic[] = "test/lol";

  // message comes in one piece
  char data[] = {0x01, 0x02, 0x03, 0x04};
  payloadHelper.onData(topic, data, properties, 4, 0, 4);

  // message is split over two packets
  char firstChunk[] = {'h', 'e', 'l', 'l', 'o'};
  payloadHelper.onData(topic, firstChunk, properties, 5, 0, 11);
  char secondChunk[] = {' ', 'w', 'o', 'r', 'l', 'd'};  // no zero termination!
  payloadHelper.onData(topic, secondChunk, properties, 6, 5, 11);

  // message is larger than max
  char bigData[20];
  payloadHelper.onData(topic, bigData, properties, 20,  0, 60);
  payloadHelper.onData(topic, bigData, properties, 20, 20, 60);
  payloadHelper.onData(topic, bigData, properties, 20, 40, 60);

  // First method to use AsyncMqttPayloadHelper: call its onData function within a regular
  // AsyncMqttClient onMessage function. Here using a lambda, could also be a regular function
  mqttClient.onMessage([](char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    payloadHelper.onData(topic, payload, properties, len, index, total);
  });

  // Second method: use std::bind to attach a objects member function
  mqttClient.onMessage(std::bind(&AsyncMqttPayloadHelper::onData, &payloadHelper, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
}

void loop() {
  yield();
}
