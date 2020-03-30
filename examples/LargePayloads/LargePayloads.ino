#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>

#include <AsyncMqttClient.h>

#define WIFI_SSID "My_Wi-Fi"
#define WIFI_PASSWORD "my-awesome-password"

#define MQTT_HOST IPAddress(192, 168, 1, 10)
#define MQTT_PORT 1883

extern const uint8_t image[3039];

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;
bool sendFile = false;
uint32_t lastMillis = 0;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

const char* readLongFile(size_t index) {
  return reinterpret_cast<const char*>(&image[index]);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  sendFile = true;
  lastMillis = millis();
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.printf("Disconnected from MQTT: %d\n", reason);
  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin("WIFI_SSID", "WIFI_PASSWORD");
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void setup() {
  Serial.begin(74880);
  Serial.println();
  Serial.println();

  delay(5000);

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWifi();
}

void loop() {
  delay(1);
  if (millis() - lastMillis > 10000 && sendFile) {
    sendFile = false;
    mqttClient.publish("testtopic/image", 0, false, readLongFile, 3039);
  }
}

const uint8_t image[3039] = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
  0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
  0x00, 0x00, 0x00, 0xe1, 0x00, 0x00, 0x00, 0xe1,
  0x08, 0x03, 0x00, 0x00, 0x00, 0x09, 0x6d, 0x22,
  0x48, 0x00, 0x00, 0x00, 0x6f, 0x50, 0x4c, 0x54,
  0x45, 0xff, 0xff, 0xff, 0x07, 0x07, 0x07, 0xfc,
  0xfc, 0xfc, 0xe6, 0xe6, 0xe6, 0xbe, 0xbe, 0xbe,
  0xd4, 0xd4, 0xd4, 0xf4, 0xf4, 0xf4, 0x28, 0x28,
  0x28, 0xee, 0xee, 0xee, 0xdf, 0xdf, 0xdf, 0x99,
  0x99, 0x99, 0xc7, 0xc7, 0xc7, 0x70, 0x70, 0x70,
  0x15, 0x15, 0x15, 0x4f, 0x4f, 0x4f, 0x5e, 0x5e,
  0x5e, 0xcc, 0xcc, 0xcc, 0xb4, 0xb4, 0xb4, 0x8b,
  0x8b, 0x8b, 0x31, 0x31, 0x31, 0x66, 0x66, 0x66,
  0x44, 0x44, 0x44, 0xa9, 0xa9, 0xa9, 0xa3, 0xa3,
  0xa3, 0x6c, 0x6c, 0x6c, 0x7a, 0x7a, 0x7a, 0x21,
  0x21, 0x21, 0x0e, 0x0e, 0x0e, 0xc2, 0xc2, 0xc2,
  0x59, 0x59, 0x59, 0x37, 0x37, 0x37, 0x1a, 0x1a,
  0x1a, 0x83, 0x83, 0x83, 0x94, 0x94, 0x94, 0x3c,
  0x3c, 0x3c, 0x78, 0x78, 0x78, 0x8c, 0x8c, 0x8c,
  0xa8, 0x8d, 0xba, 0xcb, 0x00, 0x00, 0x0b, 0x2b,
  0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0xec, 0xda,
  0xe9, 0x7a, 0xa2, 0x30, 0x14, 0x06, 0xe0, 0x73,
  0x12, 0x92, 0x10, 0xf6, 0x45, 0x44, 0x65, 0x71,
  0xbd, 0xff, 0x6b, 0x9c, 0xd1, 0x3a, 0x24, 0x98,
  0x68, 0x6d, 0x45, 0xd4, 0x79, 0x78, 0xff, 0xd5,
  0x52, 0x9e, 0x7e, 0xe4, 0xe4, 0x90, 0xa4, 0x85,
  0xc9, 0x64, 0x32, 0x99, 0x4c, 0x26, 0x93, 0xc9,
  0x64, 0x32, 0x99, 0x4c, 0x26, 0x93, 0xc9, 0x64,
  0x32, 0x99, 0x4c, 0x26, 0x93, 0xc9, 0x64, 0x32,
  0x99, 0x4c, 0x7e, 0x88, 0x50, 0x56, 0x3b, 0x4a,
  0xcd, 0x28, 0x81, 0xff, 0x05, 0x71, 0x13, 0x2f,
  0x58, 0xc4, 0x45, 0x2b, 0x56, 0x8a, 0x68, 0xa3,
  0x78, 0x11, 0x78, 0x89, 0xfb, 0xe9, 0x39, 0x89,
  0xef, 0xed, 0x73, 0xbc, 0x45, 0x2c, 0x3c, 0xc7,
  0x85, 0x8f, 0x44, 0x5c, 0x99, 0x2e, 0x76, 0x78,
  0x8f, 0xbc, 0x29, 0x19, 0xe5, 0xf0, 0x59, 0x5c,
  0x56, 0x06, 0x02, 0x7f, 0x20, 0xca, 0xea, 0x0f,
  0x9a, 0x99, 0x44, 0xa6, 0xcb, 0x1c, 0x7f, 0x6a,
  0x15, 0x79, 0x3e, 0x85, 0x4f, 0xc0, 0xfd, 0x43,
  0x13, 0xe2, 0xaf, 0xe4, 0x41, 0xc9, 0xe0, 0xdd,
  0x71, 0x27, 0xd8, 0x55, 0xf8, 0x6b, 0xeb, 0x28,
  0x7b, 0xef, 0x8c, 0xc4, 0x99, 0x09, 0x7c, 0x50,
  0xbb, 0x79, 0xdf, 0x8c, 0xc4, 0x69, 0x2a, 0x1c,
  0x80, 0x58, 0xbe, 0x67, 0x46, 0x5e, 0xc7, 0x38,
  0x94, 0xf5, 0x46, 0xbe, 0x5d, 0x63, 0x25, 0x6c,
  0x8f, 0x43, 0x12, 0xe5, 0x9b, 0xbd, 0x3c, 0xa4,
  0x97, 0xe3, 0xc0, 0x62, 0xe7, 0x8d, 0x16, 0x01,
  0xdc, 0x89, 0x2b, 0x1c, 0x9c, 0x78, 0x9f, 0x96,
  0x43, 0x0f, 0x39, 0x3e, 0x45, 0x93, 0xbe, 0x47,
  0xa5, 0x26, 0x4d, 0x88, 0x4f, 0x22, 0x36, 0xef,
  0x50, 0xa9, 0xa9, 0xc0, 0x27, 0x8a, 0x5f, 0xbe,
  0x90, 0xe3, 0x1b, 0x7c, 0x2e, 0x51, 0x13, 0x78,
  0x25, 0xba, 0xc4, 0x67, 0xcb, 0x53, 0x0e, 0xaf,
  0xc3, 0x16, 0xf8, 0x7c, 0xc2, 0x73, 0xe1, 0x55,
  0xfc, 0x06, 0xc7, 0xb0, 0xca, 0x28, 0xbc, 0x46,
  0x5d, 0xe0, 0x48, 0x82, 0xd7, 0x44, 0x74, 0x5a,
  0x1c, 0xcd, 0xcc, 0x85, 0xf1, 0xd5, 0x2b, 0x1c,
  0xd1, 0x8c, 0xc2, 0xc8, 0xc8, 0x3c, 0xc4, 0x51,
  0x35, 0x12, 0x46, 0xc5, 0x4b, 0x81, 0x23, 0x6b,
  0x12, 0x78, 0x0a, 0xe2, 0x52, 0x4a, 0x25, 0x4b,
  0x12, 0xbf, 0x67, 0x2b, 0x70, 0x74, 0x4d, 0xea,
  0xf7, 0x24, 0x8c, 0x49, 0x4a, 0x29, 0x87, 0x07,
  0x70, 0xe6, 0x94, 0x9b, 0xc5, 0x2c, 0x8e, 0x8a,
  0x5d, 0x9b, 0x0b, 0x5d, 0x88, 0x2f, 0xb0, 0x12,
  0xba, 0xbc, 0xdd, 0x15, 0xd1, 0x6c, 0x16, 0x1c,
  0xd2, 0x5a, 0x92, 0x5f, 0xc6, 0x2b, 0x97, 0xc5,
  0x2a, 0xc4, 0xb7, 0xb7, 0x16, 0x71, 0x96, 0xca,
  0x5f, 0x6c, 0xf9, 0xf6, 0xc5, 0x07, 0xa4, 0xfb,
  0x47, 0x34, 0x5b, 0x06, 0x3f, 0x41, 0xd2, 0x58,
  0xe0, 0x67, 0xa9, 0xda, 0x25, 0xfb, 0x41, 0xbe,
  0x08, 0x3f, 0x51, 0x78, 0x67, 0x46, 0xe2, 0xc7,
  0xf8, 0xa9, 0x56, 0x07, 0x09, 0xdf, 0xa2, 0xde,
  0xa7, 0xd5, 0x67, 0x4f, 0xe3, 0x70, 0xb8, 0x8d,
  0x05, 0xf8, 0x8e, 0xaa, 0xb3, 0xc7, 0xf7, 0x5a,
  0x7e, 0x84, 0x2f, 0xb3, 0x2e, 0xe2, 0xe0, 0x64,
  0xb9, 0xd9, 0x64, 0xd9, 0x61, 0xbb, 0xf5, 0xbc,
  0xb2, 0x2c, 0xe7, 0xf3, 0x79, 0xaa, 0xfb, 0xfb,
  0x75, 0x59, 0x7a, 0xdb, 0x43, 0xb6, 0x0c, 0x82,
  0x20, 0x0a, 0xd1, 0x50, 0x6d, 0x28, 0x5c, 0x97,
  0xee, 0xf0, 0x45, 0xf2, 0xcc, 0x67, 0x92, 0xba,
  0x67, 0xfc, 0x84, 0x9c, 0x41, 0x0f, 0x39, 0xe2,
  0x9c, 0xbb, 0x7f, 0x49, 0xe6, 0x6f, 0x0b, 0xbc,
  0xb4, 0x90, 0xd7, 0x03, 0xe6, 0xf8, 0x1a, 0x8b,
  0xda, 0x25, 0xbf, 0x5e, 0x56, 0x32, 0x63, 0x09,
  0x39, 0x93, 0xb7, 0x4e, 0xcd, 0xc6, 0x57, 0x2d,
  0x29, 0x3c, 0x86, 0xf8, 0x4d, 0x75, 0xcf, 0x5e,
  0xcb, 0x7f, 0x49, 0x89, 0x8a, 0x40, 0xc2, 0x00,
  0x58, 0x20, 0xbe, 0x3d, 0x14, 0x60, 0x11, 0x8e,
  0x2f, 0x6c, 0x7c, 0x18, 0x48, 0x3d, 0x43, 0xcd,
  0x81, 0xc3, 0x25, 0xba, 0xc0, 0xf1, 0x89, 0x2d,
  0x85, 0xc1, 0x50, 0x7d, 0x3e, 0xae, 0x4b, 0x02,
  0x7d, 0x64, 0x83, 0xe3, 0x2b, 0x1c, 0x0e, 0x43,
  0x72, 0x1a, 0xec, 0xac, 0x6a, 0xe8, 0x4b, 0x2b,
  0x1c, 0x5d, 0x24, 0x61, 0x60, 0x2c, 0xd6, 0xee,
  0x4e, 0x41, 0x47, 0x73, 0x1c, 0x5d, 0x44, 0x61,
  0x70, 0x72, 0x86, 0x9d, 0x0d, 0x01, 0x85, 0xec,
  0x71, 0x74, 0xb1, 0x0b, 0x4f, 0x20, 0x1b, 0x55,
  0xa7, 0x29, 0x28, 0xf7, 0x1f, 0x9c, 0xbd, 0xfd,
  0x09, 0xa8, 0x8c, 0x8c, 0x59, 0xa0, 0x82, 0x8f,
  0x29, 0x96, 0xf0, 0x24, 0x6c, 0x85, 0x67, 0xe1,
  0x96, 0xc0, 0x17, 0xe2, 0x85, 0x38, 0xb2, 0x88,
  0xc1, 0x10, 0x88, 0x4c, 0x12, 0x4a, 0xa0, 0xcf,
  0x53, 0xbd, 0x3a, 0x79, 0xe6, 0xbb, 0x5e, 0x04,
  0xde, 0x5f, 0x11, 0x5a, 0x89, 0x39, 0x0c, 0x81,
  0x6d, 0x63, 0x91, 0xcf, 0x3c, 0x76, 0x11, 0x5b,
  0x75, 0x9b, 0x8c, 0x9f, 0x87, 0xb0, 0xc2, 0xc1,
  0xcd, 0x7c, 0xf7, 0xf4, 0x8c, 0xcb, 0x1c, 0x2d,
  0x96, 0x04, 0x1e, 0xa6, 0x0e, 0x5b, 0xa2, 0x14,
  0x7a, 0x12, 0x81, 0x67, 0xbb, 0x44, 0xcd, 0xcd,
  0x81, 0x2d, 0xdd, 0x6e, 0x51, 0x1c, 0xa1, 0x21,
  0x97, 0xf0, 0x38, 0xbe, 0xed, 0x26, 0x9c, 0x28,
  0x49, 0x2f, 0xfa, 0x01, 0xff, 0xd9, 0x72, 0x00,
  0x20, 0xf3, 0x0a, 0xfb, 0xda, 0x45, 0xd0, 0x89,
  0xb5, 0x6f, 0xc6, 0x81, 0x52, 0x74, 0x9f, 0xe7,
  0x7b, 0xed, 0xea, 0x1c, 0x4f, 0x1a, 0x0e, 0x9d,
  0x5a, 0xe0, 0xa5, 0xcc, 0x18, 0x8e, 0xfa, 0x90,
  0xdd, 0xa9, 0xa4, 0x60, 0x56, 0x5e, 0xeb, 0x83,
  0x8e, 0x09, 0x3c, 0x2b, 0x8e, 0x57, 0xbb, 0x33,
  0xbc, 0x50, 0x70, 0x50, 0xfc, 0x02, 0xcf, 0x56,
  0xf6, 0x9d, 0xd6, 0x06, 0x34, 0xe4, 0xf4, 0xb1,
  0x90, 0xa0, 0x31, 0x27, 0x81, 0x59, 0x70, 0x5b,
  0xbc, 0xd7, 0xca, 0x81, 0x93, 0x7a, 0x7d, 0xbd,
  0xec, 0x49, 0x89, 0xff, 0xcc, 0x01, 0x20, 0x31,
  0x6f, 0x22, 0x41, 0x73, 0xe8, 0x82, 0x83, 0x46,
  0x76, 0xc1, 0x53, 0xd0, 0x38, 0xeb, 0x53, 0x68,
  0x0e, 0x1a, 0x56, 0x60, 0xdf, 0xc6, 0x4c, 0xe8,
  0xe1, 0xbd, 0xc4, 0x57, 0x42, 0x7e, 0xb8, 0xd5,
  0x9b, 0x93, 0x1d, 0x9e, 0x35, 0xc4, 0x7a, 0xf3,
  0x14, 0x34, 0xf3, 0xf5, 0xbf, 0x8b, 0x41, 0xd7,
  0x8d, 0x7c, 0x2f, 0xcd, 0x06, 0x8f, 0x4a, 0x02,
  0xb7, 0x16, 0x4c, 0x6c, 0x80, 0x84, 0x34, 0x36,
  0x3f, 0x55, 0xdc, 0x65, 0x37, 0x5a, 0x0c, 0x48,
  0x84, 0x86, 0xad, 0x75, 0x08, 0x0e, 0xa0, 0x0b,
  0xac, 0x25, 0xb7, 0xc0, 0x23, 0xef, 0x66, 0xc2,
  0x82, 0x0f, 0x90, 0xd0, 0x0d, 0x8c, 0xda, 0xd5,
  0xa5, 0x5d, 0x17, 0x2a, 0x81, 0x0a, 0x34, 0x04,
  0x04, 0x14, 0x1a, 0x59, 0x1f, 0x7d, 0x86, 0x5f,
  0x72, 0xcb, 0x92, 0x69, 0xe1, 0xde, 0xda, 0x5a,
  0x67, 0x64, 0x80, 0x84, 0x64, 0x6e, 0x54, 0xa9,
  0x7d, 0x79, 0x1a, 0x80, 0x83, 0x26, 0xc1, 0x41,
  0x71, 0x63, 0xfc, 0x42, 0xc1, 0x9c, 0x71, 0x88,
  0x7b, 0xd0, 0xf8, 0x2d, 0x1e, 0xad, 0x7d, 0xd0,
  0x94, 0x21, 0xf6, 0x24, 0x30, 0x40, 0x42, 0x48,
  0xda, 0x5b, 0x2f, 0x58, 0xd2, 0x95, 0x69, 0x0b,
  0x25, 0x5a, 0xb8, 0xa0, 0x39, 0x5f, 0xbc, 0x76,
  0x8d, 0xd7, 0xaa, 0x39, 0x67, 0xcf, 0xc5, 0x51,
  0xb8, 0xd7, 0x1b, 0x4d, 0x28, 0x07, 0x49, 0x48,
  0x0e, 0xa8, 0xe4, 0x35, 0x5c, 0x50, 0x0b, 0x51,
  0xb0, 0xde, 0xbb, 0xb6, 0x74, 0xfb, 0x19, 0x07,
  0x9d, 0xcc, 0xd5, 0xa5, 0xe6, 0x93, 0x8b, 0xc8,
  0xd5, 0x4e, 0x1a, 0xd1, 0x41, 0x12, 0xea, 0x33,
  0xb1, 0xda, 0xc2, 0xa5, 0x3a, 0xef, 0x12, 0x6e,
  0xd1, 0x22, 0xb6, 0x54, 0x99, 0x47, 0x40, 0x47,
  0x22, 0xb3, 0xe6, 0x78, 0xa0, 0x4d, 0x0c, 0x4e,
  0x00, 0x88, 0x7b, 0x1e, 0x55, 0xcd, 0x81, 0x0f,
  0x93, 0x10, 0xdc, 0x8d, 0xc0, 0xa3, 0x70, 0x37,
  0x07, 0x83, 0xaf, 0x12, 0x66, 0x68, 0xd1, 0x5a,
  0x4a, 0xaf, 0x24, 0x60, 0x69, 0xa6, 0x82, 0x5e,
  0x39, 0x8f, 0x5c, 0x2d, 0xd3, 0xda, 0x99, 0x47,
  0x68, 0x28, 0xc9, 0x40, 0x09, 0x81, 0xf8, 0xcb,
  0xe0, 0x2f, 0x4f, 0x82, 0x89, 0xed, 0xba, 0x84,
  0x4b, 0xb4, 0x58, 0xbb, 0xe6, 0xe3, 0x70, 0x40,
  0x51, 0xaf, 0xbe, 0x88, 0x5f, 0xdf, 0x48, 0x57,
  0x68, 0x33, 0x7f, 0x3c, 0xa1, 0x62, 0x1c, 0xfa,
  0x9b, 0x2b, 0x12, 0x08, 0xd0, 0xd4, 0x8f, 0xc3,
  0x4f, 0x17, 0x0b, 0x06, 0x7d, 0x73, 0x3c, 0x8a,
  0xf9, 0x4f, 0xd7, 0x5e, 0xe9, 0x63, 0x09, 0xdb,
  0x1a, 0xee, 0x41, 0xa3, 0x2e, 0xe1, 0x0c, 0x6d,
  0x4a, 0xd0, 0x9c, 0x2e, 0x2e, 0x24, 0xf4, 0x51,
  0x3c, 0x0a, 0xb4, 0x84, 0x74, 0x86, 0xdf, 0x0b,
  0x1d, 0xf8, 0x36, 0x61, 0xe4, 0xb0, 0xeb, 0x38,
  0xdc, 0xc3, 0x55, 0x09, 0x63, 0xb4, 0x99, 0x81,
  0xa6, 0xb1, 0xff, 0xb3, 0xae, 0xea, 0x3f, 0xd6,
  0x57, 0xfb, 0x5a, 0x9c, 0xe4, 0xa2, 0x42, 0x9d,
  0x48, 0xbe, 0x4f, 0x18, 0x53, 0xb8, 0x03, 0x61,
  0xbe, 0x93, 0x7e, 0x71, 0x8c, 0xd8, 0xbc, 0xe9,
  0x12, 0x46, 0x68, 0x53, 0x10, 0x50, 0x16, 0xdd,
  0x4e, 0xab, 0x27, 0xbc, 0x6c, 0xa5, 0xce, 0xda,
  0xf6, 0xf7, 0x2d, 0x27, 0x44, 0x5d, 0xc1, 0xee,
  0x48, 0x28, 0xef, 0xc8, 0xe7, 0x89, 0x75, 0x58,
  0x7d, 0xb1, 0x94, 0x2e, 0x89, 0xed, 0x09, 0xed,
  0x7b, 0x54, 0x4f, 0xcd, 0x4c, 0x63, 0x68, 0xa5,
  0x75, 0xcf, 0xc2, 0xe0, 0xe9, 0x09, 0xf9, 0xd2,
  0x68, 0x3f, 0xf6, 0x9d, 0x01, 0x14, 0x68, 0x13,
  0x96, 0xa0, 0x30, 0x44, 0xac, 0xcc, 0x7b, 0x1c,
  0x03, 0xad, 0x25, 0x74, 0xb4, 0x0d, 0x8d, 0x3f,
  0x72, 0xc2, 0xdc, 0xfc, 0xed, 0x16, 0x5d, 0xc2,
  0x16, 0xad, 0x36, 0x17, 0x3d, 0xa5, 0x35, 0xa7,
  0x8f, 0x83, 0x88, 0x3b, 0xa9, 0x37, 0xe8, 0xf7,
  0x4c, 0x98, 0xa3, 0xd5, 0x82, 0x43, 0xc7, 0xb5,
  0xff, 0xcd, 0xd1, 0x47, 0xc4, 0x40, 0x5f, 0x80,
  0xaa, 0x28, 0xc9, 0xf5, 0x84, 0xd1, 0x40, 0x09,
  0x5d, 0x4b, 0x42, 0xfb, 0xee, 0x4e, 0xa0, 0xc6,
  0xfe, 0xa8, 0x89, 0x50, 0x47, 0x4b, 0x1a, 0xba,
  0xea, 0xb7, 0xd2, 0x14, 0xef, 0x4a, 0x28, 0xdf,
  0x24, 0x61, 0xef, 0x67, 0x62, 0x75, 0x30, 0xa1,
  0x21, 0x45, 0x6f, 0x09, 0xc6, 0x97, 0x6f, 0x9a,
  0x70, 0x85, 0x56, 0x95, 0x07, 0xca, 0x1e, 0xd1,
  0x03, 0x03, 0x69, 0x7a, 0x4b, 0x30, 0x1e, 0x7f,
  0x56, 0xc2, 0xde, 0x9e, 0xf2, 0xa0, 0xaf, 0xe8,
  0xbb, 0x2d, 0x3a, 0xc9, 0x7a, 0x0b, 0x14, 0x2a,
  0xee, 0x4a, 0xd8, 0xdc, 0x97, 0x90, 0xd8, 0xfd,
  0x26, 0xe1, 0x1a, 0x0d, 0xc6, 0x49, 0x84, 0xa3,
  0x6d, 0x31, 0x53, 0xc1, 0x55, 0xab, 0x69, 0x99,
  0xde, 0x78, 0x06, 0x4b, 0xb8, 0xcb, 0xbc, 0x2b,
  0xb4, 0x92, 0x79, 0x20, 0xa1, 0x79, 0xc8, 0xca,
  0xb5, 0xc6, 0x93, 0xa9, 0x77, 0x3f, 0xed, 0x35,
  0xc6, 0xf9, 0x30, 0x09, 0x6f, 0xa3, 0x03, 0x26,
  0xc4, 0x14, 0x3a, 0x44, 0x8d, 0x28, 0xdf, 0x63,
  0xa6, 0xea, 0x52, 0xff, 0x75, 0x9b, 0x8f, 0x4b,
  0xf8, 0xa7, 0xbd, 0x3b, 0x5d, 0x4f, 0x14, 0x86,
  0xc2, 0x00, 0x7c, 0x4e, 0x80, 0xb0, 0xaf, 0xb2,
  0xa3, 0x2c, 0xda, 0xfb, 0xbf, 0xc6, 0xa9, 0x76,
  0x14, 0x6c, 0x02, 0x13, 0x46, 0xa1, 0xd0, 0x27,
  0xef, 0xbf, 0xfe, 0xa9, 0xfd, 0x0c, 0x0d, 0xe1,
  0x84, 0x24, 0x83, 0x2b, 0x82, 0x9c, 0xdb, 0xc1,
  0xe8, 0xda, 0x81, 0x47, 0xda, 0x61, 0x0f, 0xeb,
  0xee, 0x2e, 0xe1, 0xf0, 0x1e, 0x9f, 0x65, 0x83,
  0xfa, 0x47, 0xd2, 0xaf, 0xb9, 0x08, 0x87, 0x03,
  0x9f, 0xdd, 0x25, 0x7c, 0xba, 0xe7, 0xd3, 0xc1,
  0x7c, 0x45, 0xf5, 0xf8, 0x81, 0x52, 0x78, 0xc8,
  0x36, 0x9b, 0xb0, 0x9c, 0xf7, 0xa8, 0xda, 0xdd,
  0x3e, 0x86, 0xc3, 0x59, 0x3b, 0x61, 0xfb, 0xe2,
  0x98, 0x86, 0x2d, 0x19, 0xf5, 0xbf, 0x57, 0x03,
  0x0e, 0x4f, 0x30, 0xa1, 0xbf, 0xa1, 0x84, 0x11,
  0x85, 0xde, 0xb0, 0x48, 0xd9, 0x01, 0x4b, 0x69,
  0x36, 0x9b, 0xf0, 0x80, 0x63, 0xce, 0xca, 0xc8,
  0xeb, 0x7d, 0x31, 0xb0, 0x82, 0x6a, 0xb3, 0x09,
  0x6b, 0x1c, 0x65, 0x02, 0xc3, 0x2e, 0x11, 0xd1,
  0x03, 0x56, 0x87, 0xa2, 0x09, 0x15, 0x5e, 0xc2,
  0xec, 0xc5, 0x84, 0xc7, 0x89, 0x84, 0x67, 0x1c,
  0x15, 0x02, 0x43, 0x1d, 0x99, 0x1e, 0x83, 0x58,
  0x34, 0x61, 0xac, 0x00, 0xc7, 0xfb, 0x13, 0xe6,
  0x6c, 0x15, 0x83, 0xe5, 0x10, 0x7e, 0x42, 0xdd,
  0x06, 0xc6, 0x49, 0x34, 0x61, 0xbe, 0x52, 0xc2,
  0x98, 0xad, 0x44, 0xb1, 0x3c, 0xc2, 0x5f, 0x7d,
  0xdf, 0xa4, 0x23, 0x13, 0x21, 0x22, 0x55, 0x0c,
  0x87, 0x9f, 0xb0, 0xc2, 0x01, 0xcf, 0x56, 0x46,
  0xc0, 0x03, 0x8d, 0x26, 0x13, 0x92, 0x3e, 0xe1,
  0x09, 0x47, 0x35, 0x0a, 0xd3, 0x63, 0x9e, 0xfa,
  0x67, 0xc7, 0x27, 0x99, 0x2b, 0x9a, 0xf0, 0x62,
  0x00, 0x47, 0x5a, 0x89, 0xaf, 0x17, 0x65, 0x13,
  0x4e, 0x57, 0x13, 0x63, 0x1c, 0x67, 0x8f, 0xcc,
  0x06, 0xb4, 0xf0, 0x5d, 0x84, 0xa2, 0x09, 0x5b,
  0x7e, 0x42, 0x77, 0x6e, 0x42, 0xf2, 0x94, 0xd0,
  0x9e, 0xa8, 0x08, 0xe7, 0x38, 0x8e, 0xb9, 0x1a,
  0x35, 0xbd, 0x9f, 0x4a, 0x1c, 0x32, 0x62, 0xe1,
  0x84, 0x1d, 0xf9, 0x77, 0x42, 0xff, 0x85, 0x84,
  0xec, 0xbc, 0xc5, 0x05, 0xc7, 0xf9, 0x6c, 0x47,
  0xc3, 0x9f, 0xe4, 0x34, 0x3d, 0xe1, 0x84, 0x19,
  0xf0, 0x84, 0xcd, 0xdc, 0x85, 0xdb, 0xe4, 0x83,
  0x4d, 0xc8, 0x9f, 0x7b, 0x6a, 0x71, 0x5c, 0xc5,
  0xde, 0xb6, 0xf8, 0xb5, 0x16, 0xf5, 0x20, 0x9c,
  0x30, 0x15, 0x48, 0x98, 0xbf, 0x9e, 0x50, 0xeb,
  0x13, 0x7e, 0xe0, 0x04, 0xc2, 0x0c, 0x3d, 0xf8,
  0x43, 0x88, 0x0c, 0x5f, 0x4d, 0x58, 0xce, 0xdd,
  0x42, 0x81, 0x74, 0x93, 0x09, 0x83, 0x9a, 0x99,
  0xe5, 0x16, 0xb9, 0xe7, 0x53, 0x1f, 0x91, 0xdb,
  0x99, 0x76, 0xe2, 0x09, 0x43, 0xe0, 0x51, 0x75,
  0xa6, 0xbf, 0x9d, 0x93, 0xf0, 0x6c, 0x4f, 0xcc,
  0x72, 0x5b, 0x38, 0xa1, 0x80, 0x27, 0x86, 0x8b,
  0x5f, 0x3a, 0x78, 0x62, 0xfa, 0xc2, 0x09, 0x5d,
  0x91, 0x84, 0xc5, 0xeb, 0x09, 0x43, 0xfd, 0x91,
  0x30, 0xc5, 0x09, 0x31, 0x3b, 0x29, 0x7a, 0x13,
  0x31, 0x5f, 0x98, 0x68, 0xc2, 0x52, 0x15, 0x48,
  0xd8, 0xd2, 0xb9, 0x09, 0x83, 0xd1, 0xdb, 0x57,
  0x03, 0x81, 0x8b, 0xe3, 0xce, 0xc6, 0x73, 0x92,
  0x91, 0xce, 0x4e, 0x6d, 0x84, 0x13, 0x9e, 0x6d,
  0x81, 0x84, 0x1f, 0x44, 0x20, 0xa1, 0x35, 0x95,
  0x90, 0xe6, 0xf8, 0x97, 0x07, 0x46, 0x8d, 0x7c,
  0x9c, 0xc9, 0xfb, 0x82, 0xff, 0x77, 0xd2, 0x0e,
  0x85, 0x13, 0xfa, 0xa6, 0x40, 0x42, 0x8b, 0xc0,
  0xbf, 0x31, 0x09, 0xf9, 0xab, 0xd4, 0x5a, 0x20,
  0x39, 0x8e, 0x73, 0x53, 0x18, 0xf2, 0xf8, 0x33,
  0x92, 0x46, 0x21, 0x9e, 0xf0, 0x42, 0x81, 0xc1,
  0xdc, 0x6e, 0x32, 0x98, 0x99, 0x30, 0x09, 0x46,
  0x1b, 0x38, 0x04, 0x08, 0x71, 0xc2, 0x05, 0x06,
  0x88, 0xce, 0x56, 0x38, 0xf8, 0xaf, 0x51, 0x1f,
  0x47, 0x13, 0x16, 0x44, 0x20, 0x61, 0x0a, 0x02,
  0xb2, 0x8a, 0xad, 0x9a, 0xb1, 0x5d, 0xdf, 0x41,
  0x01, 0x30, 0x0f, 0xa2, 0x4b, 0x93, 0x34, 0x7e,
  0x6f, 0xd7, 0xff, 0x2b, 0x0b, 0x3c, 0x3d, 0xb5,
  0xfc, 0x84, 0xc7, 0xf7, 0x24, 0x64, 0x3f, 0xf3,
  0x42, 0xfa, 0x41, 0x3a, 0x5f, 0x4b, 0xb8, 0xc3,
  0xeb, 0x7a, 0x98, 0xe1, 0x88, 0xdf, 0x15, 0xa9,
  0x6a, 0x6b, 0x0a, 0xbd, 0x22, 0xe1, 0x30, 0x61,
  0x99, 0x02, 0x97, 0xe6, 0x95, 0x9f, 0x9a, 0x2b,
  0xd7, 0x6d, 0xc4, 0x12, 0x22, 0x9b, 0x90, 0x79,
  0xe5, 0xad, 0x0a, 0xfa, 0x57, 0xcd, 0x47, 0xb8,
  0x91, 0x1d, 0xdc, 0xd8, 0x76, 0x87, 0x03, 0x75,
  0x76, 0xfc, 0x62, 0xab, 0x69, 0x8e, 0x7c, 0x95,
  0x7b, 0x83, 0xcc, 0x83, 0x1c, 0x8b, 0x6a, 0xaa,
  0xaa, 0x5e, 0x57, 0x6a, 0xdf, 0x28, 0x20, 0x40,
  0xf5, 0x4f, 0x27, 0x3f, 0xce, 0x2f, 0x45, 0xd4,
  0x59, 0x21, 0x05, 0xe0, 0x7e, 0xe5, 0xb1, 0xb1,
  0xf6, 0xda, 0xca, 0x44, 0x83, 0x77, 0x21, 0xb7,
  0x35, 0xdf, 0x84, 0x00, 0xc3, 0x38, 0xe1, 0x5f,
  0x55, 0x4a, 0xd6, 0x5e, 0xe1, 0x9c, 0x13, 0x58,
  0x5e, 0xc6, 0xdc, 0x9b, 0x56, 0x6c, 0xc4, 0x08,
  0x96, 0x67, 0x3e, 0x1a, 0xac, 0x49, 0xc9, 0xda,
  0xcb, 0xd4, 0x75, 0x0d, 0x16, 0x47, 0x7c, 0xce,
  0xf0, 0x82, 0x5e, 0x70, 0x1d, 0x09, 0x81, 0xc5,
  0x75, 0x78, 0xa7, 0x87, 0xf0, 0xa0, 0xd5, 0xb8,
  0x0a, 0x0b, 0x16, 0x97, 0xb9, 0xfc, 0xd1, 0x45,
  0x86, 0x6b, 0x70, 0x0d, 0x58, 0x18, 0x49, 0x75,
  0xbc, 0x4b, 0x4c, 0xa6, 0xbc, 0xc8, 0xb3, 0xaf,
  0x7e, 0x86, 0xa4, 0x07, 0xbc, 0xab, 0x42, 0xe8,
  0xad, 0x74, 0x9d, 0xd6, 0x06, 0x2c, 0x86, 0xdd,
  0x27, 0xaf, 0x20, 0x6c, 0xa5, 0x64, 0x69, 0x21,
  0x2c, 0xcb, 0x2c, 0x9a, 0xa9, 0x45, 0x0f, 0xd4,
  0xaa, 0x70, 0x59, 0x2d, 0x85, 0x45, 0x85, 0x1e,
  0xf7, 0x79, 0xaa, 0x67, 0xb4, 0x28, 0x68, 0x93,
  0x7b, 0x90, 0x9a, 0x8e, 0x8e, 0x3d, 0x5d, 0x25,
  0x00, 0x6b, 0xee, 0xfd, 0xc1, 0x56, 0x97, 0x96,
  0xbb, 0x40, 0xfb, 0xc1, 0x0c, 0xcb, 0x8c, 0x71,
  0x29, 0xb5, 0x45, 0x60, 0x29, 0xc4, 0x08, 0x7d,
  0x7c, 0x52, 0xf6, 0x9f, 0xb6, 0x52, 0xc4, 0x26,
  0x0f, 0x60, 0x11, 0x84, 0x1a, 0x66, 0xf0, 0x51,
  0xe3, 0xf7, 0x80, 0x14, 0xc6, 0x98, 0x17, 0x5c,
  0x40, 0xe5, 0x85, 0x81, 0xfd, 0x25, 0x18, 0xd0,
  0xbe, 0x31, 0x07, 0x14, 0x3e, 0xf3, 0x4a, 0xfb,
  0x14, 0x04, 0xf6, 0xf1, 0x53, 0x1a, 0x39, 0x09,
  0x7e, 0x77, 0x48, 0x09, 0x8c, 0x53, 0xa2, 0x66,
  0x81, 0x84, 0x75, 0x7d, 0xf8, 0x52, 0x7f, 0x39,
  0x5f, 0x25, 0x0f, 0xde, 0x5f, 0xa7, 0x2b, 0xff,
  0x2a, 0xbe, 0xc9, 0x9f, 0xc5, 0x9f, 0x7c, 0xff,
  0x74, 0xf2, 0xbc, 0xdb, 0x6e, 0xa3, 0xd7, 0x9a,
  0x00, 0x72, 0x24, 0x2a, 0x81, 0x29, 0x46, 0x7a,
  0xc0, 0x5d, 0xf3, 0x03, 0xf8, 0x07, 0x12, 0xec,
  0x77, 0xdb, 0x44, 0xc4, 0x46, 0x68, 0x5b, 0x26,
  0x9a, 0x26, 0xb8, 0x53, 0xb9, 0x46, 0x40, 0x88,
  0x91, 0x25, 0x0d, 0xee, 0x8e, 0x9e, 0xdb, 0x04,
  0x84, 0x19, 0x59, 0xbc, 0xaf, 0x2d, 0x30, 0xab,
  0xf3, 0xe5, 0x08, 0xf3, 0x28, 0x61, 0x9b, 0x94,
  0xb8, 0x0f, 0x95, 0xee, 0x5b, 0x36, 0x85, 0xd9,
  0xa8, 0x16, 0xb6, 0xfe, 0xf6, 0x2f, 0xd7, 0x83,
  0x97, 0x5b, 0xff, 0x7f, 0x2a, 0x9d, 0xa1, 0x68,
  0x6a, 0x98, 0x66, 0x5d, 0x5b, 0x38, 0x79, 0x1e,
  0x0f, 0x9c, 0x71, 0x7d, 0xae, 0x17, 0x0f, 0xe4,
  0xce, 0xa5, 0x88, 0xac, 0x34, 0x0d, 0x6d, 0x93,
  0x89, 0x37, 0x53, 0xbf, 0x97, 0xe6, 0x80, 0xed,
  0xe1, 0xda, 0xaa, 0xc8, 0x34, 0x9e, 0x51, 0x4a,
  0x3e, 0xc1, 0x3b, 0x6c, 0x62, 0x97, 0xc5, 0xc8,
  0x80, 0x75, 0x29, 0x3e, 0xae, 0xa9, 0xb1, 0x08,
  0xac, 0xcd, 0x58, 0x33, 0x62, 0x93, 0xc1, 0x0f,
  0x50, 0x1c, 0x5c, 0x8b, 0x9e, 0xc1, 0x8f, 0x50,
  0xda, 0x0a, 0x57, 0x51, 0xa7, 0x04, 0x7e, 0x86,
  0xf2, 0x51, 0xe2, 0x0a, 0x12, 0x75, 0x6e, 0xc0,
  0x9d, 0x1d, 0xe4, 0x71, 0x0a, 0x08, 0xfc, 0x1c,
  0x32, 0xa7, 0xcc, 0xba, 0xb5, 0x1d, 0x16, 0x05,
  0x69, 0x67, 0x5c, 0x54, 0xf1, 0xf3, 0x67, 0xcb,
  0x29, 0x8e, 0x8b, 0x8b, 0x39, 0x64, 0xb0, 0x01,
  0xd4, 0x5a, 0xaa, 0x19, 0x5d, 0xff, 0x08, 0x9b,
  0x40, 0x8e, 0xb9, 0x8b, 0x0b, 0xa8, 0x3f, 0x4c,
  0xd8, 0x0a, 0x65, 0xba, 0x19, 0x7f, 0xc1, 0x41,
  0xa4, 0x40, 0x34, 0x07, 0xdf, 0x4b, 0xcf, 0x7e,
  0xfc, 0x6c, 0xc7, 0x6f, 0xa8, 0x9a, 0xe0, 0x1b,
  0x39, 0xdb, 0x3b, 0x10, 0xf8, 0x9d, 0x35, 0x3a,
  0x37, 0x0f, 0x60, 0x9b, 0x8c, 0xcc, 0x6b, 0xf0,
  0x65, 0xba, 0x63, 0x6f, 0xb0, 0xfd, 0xee, 0x94,
  0xec, 0x54, 0xe2, 0x4b, 0xea, 0xe2, 0xb8, 0xa5,
  0x0e, 0x86, 0x43, 0x49, 0x9d, 0xfa, 0x85, 0x52,
  0x4c, 0x74, 0xdc, 0x70, 0xfb, 0xdd, 0x29, 0x76,
  0xf7, 0x7f, 0x17, 0xeb, 0x21, 0x4f, 0xb5, 0x8d,
  0xb7, 0xdf, 0x1d, 0x35, 0xd5, 0xa2, 0xc6, 0x99,
  0x4e, 0x56, 0xb0, 0xb1, 0xc3, 0xc6, 0x27, 0x91,
  0xcf, 0x90, 0x97, 0x03, 0x8a, 0x6a, 0x92, 0x2e,
  0x30, 0x76, 0x14, 0xef, 0x8e, 0xda, 0x96, 0x5f,
  0x89, 0x1c, 0xf8, 0x9f, 0x6a, 0x3b, 0x4c, 0x77,
  0x67, 0x68, 0xd9, 0x25, 0x4e, 0xf4, 0xb2, 0x71,
  0x99, 0x5e, 0xa5, 0x2c, 0x6b, 0x2f, 0x6e, 0xc3,
  0x3d, 0x5d, 0x9a, 0x63, 0x88, 0x72, 0x54, 0xd3,
  0xae, 0xcd, 0xfd, 0x01, 0x27, 0xca, 0x42, 0x55,
  0x33, 0xe0, 0x37, 0x21, 0xd4, 0xe8, 0x51, 0xf2,
  0x0b, 0x5a, 0x4e, 0x92, 0x24, 0x49, 0x92, 0x24,
  0x49, 0x92, 0x24, 0x49, 0x92, 0x24, 0x49, 0x92,
  0x24, 0x49, 0x92, 0x24, 0x49, 0x92, 0x24, 0x69,
  0x6b, 0xfe, 0x00, 0xb2, 0xb7, 0xe2, 0x0d, 0x46,
  0x25, 0xb6, 0x4b, 0x00, 0x00, 0x00, 0x00, 0x49,
  0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
