#pragma once

namespace AsyncMqttClientInternals {
constexpr struct {
  const char RESERVED    = 0;
  const char CONNECT     = 1;
  const char CONNACK     = 2;
  const char PUBLISH     = 3;
  const char PUBACK      = 4;
  const char PUBREC      = 5;
  const char PUBREL      = 6;
  const char PUBCOMP     = 7;
  const char SUBSCRIBE   = 8;
  const char SUBACK      = 9;
  const char UNSUBSCRIBE = 10;
  const char UNSUBACK    = 11;
  const char PINGREQ     = 12;
  const char PINGRESP    = 13;
  const char DISCONNECT  = 14;
  const char RESERVED2   = 1;
} PacketType;

constexpr struct {
  const char CONNECT_RESERVED     = 0x00;
  const char CONNACK_RESERVED     = 0x00;
  const char PUBLISH_DUP          = 0x08;
  const char PUBLISH_QOS0         = 0x00;
  const char PUBLISH_QOS1         = 0x02;
  const char PUBLISH_QOS2         = 0x04;
  const char PUBLISH_QOSRESERVED  = 0x06;
  const char PUBLISH_RETAIN       = 0x01;
  const char PUBACK_RESERVED      = 0x00;
  const char PUBREC_RESERVED      = 0x00;
  const char PUBREL_RESERVED      = 0x02;
  const char PUBCOMP_RESERVED     = 0x00;
  const char SUBSCRIBE_RESERVED   = 0x02;
  const char SUBACK_RESERVED      = 0x00;
  const char UNSUBSCRIBE_RESERVED = 0x02;
  const char UNSUBACK_RESERVED    = 0x00;
  const char PINGREQ_RESERVED     = 0x00;
  const char PINGRESP_RESERVED    = 0x00;
  const char DISCONNECT_RESERVED  = 0x00;
  const char RESERVED2_RESERVED   = 0x00;
} HeaderFlag;

constexpr struct {
  const char USERNAME      = 0x80;
  const char PASSWORD      = 0x40;
  const char WILL_RETAIN   = 0x20;
  const char WILL_QOS0     = 0x00;
  const char WILL_QOS1     = 0x08;
  const char WILL_QOS2     = 0x10;
  const char WILL          = 0x04;
  const char CLEAN_SESSION = 0x02;
  const char RESERVED      = 0x00;
} ConnectFlag;
}  // namespace AsyncMqttClientInternals
