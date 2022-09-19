#include <stdlib.h>
#include <string.h>
#include <stdint.h>

//#include <Arduino.h>
#include "esp32-hal-log.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"

#include "WebsocketFilter.hpp"

using AsyncMqttClientInternals::WebsocketFilter;

#define MIN_BUFSIZ 256

#define WS_OP_DATA_CONT     0
#define WS_OP_DATA_TEXT     1
#define WS_OP_DATA_BINARY   2
#define WS_OP_CTRL_START    8
#define WS_OP_CTRL_CLOSE    8
#define WS_OP_CTRL_PING     9
#define WS_OP_CTRL_PONG     10

WebsocketFilter::WebsocketFilter(const char * hostname, const char * wsurl,
    uint32_t n_protos, const char * protos[],
    size_t rxbufsiz, size_t txbufsiz)
{
    _state = HANDSHAKE_TX;  // Starting handshake process
    _err = NO_ERROR;        // No error (yet)

    if (rxbufsiz < MIN_BUFSIZ) rxbufsiz = MIN_BUFSIZ;
    if (txbufsiz < MIN_BUFSIZ) txbufsiz = MIN_BUFSIZ;
    _txbuf = new uint8_t[txbufsiz]; _txbufsiz = txbufsiz; _txused = 0;
    _rxbuf = new uint8_t[rxbufsiz]; _rxbufsiz = rxbufsiz; _rxused = 0;

    // Initialize header response check variables
    _num_respHdrs = 0;
    _hs_upgrade_websocket = false;
    _hs_connection_upgrade = false;
    _hs_sec_websocket_accept = false;

    _rx_inheader = false;
    _rx_textframe = false;
    _rx_binframe = false;
    _rx_lastframe = true;
    _rx_packetoffset = 0;
    _rx_maskidx = 0;
    _rx_close_code = 0;
    _rx_close_reason = NULL;
    _pendingPong = false;
    _pongData = NULL;
    _pongDataLen = 0;

    _tx_opcode = WS_OP_DATA_CONT;
    _tx_pktopen = false;

    // Calculate a 16-byte random value for Sec-WebSocket-Key header
    uint32_t randomkey[4]; size_t dummy_olen;
    for (auto i = 0; i < 4; i++) randomkey[i] = (uint32_t)random();
    memset(_base64_key, 0, sizeof(_base64_key));
    mbedtls_base64_encode(
        (unsigned char *)_base64_key, sizeof(_base64_key),
        &dummy_olen,
        (unsigned char *)randomkey, 16);

    // Start at beginning of first handshake string
    _sent_handshake[0] = 0;
    _sent_handshake[1] = 0;

    // Allocate list of handshake strings to send
    _num_handshakeStrings = 8;
    if (n_protos > 0 && protos != NULL) _num_handshakeStrings += 1 + 2 * n_protos;
    _handshakeStrings = new const char *[_num_handshakeStrings];

    _handshakeStrings[0] = "GET ";
    _handshakeStrings[1] = wsurl;
    _handshakeStrings[2] = " HTTP/1.1\r\nHost: ";
    _handshakeStrings[3] = hostname;
    _handshakeStrings[4] = "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: ";
    _handshakeStrings[5] = _base64_key;
    _handshakeStrings[6] = "\r\nSec-WebSocket-Version: 13\r\n";
    auto idx = 7;
    if (n_protos > 0 && protos != NULL) {
        _handshakeStrings[idx] = "Sec-WebSocket-Protocol: ";
        idx++;
        for (auto i = 0; i < n_protos; i++) {
            _handshakeStrings[idx] = protos[i];
            idx++;
            _handshakeStrings[idx] = (i < n_protos - 1) ? ", " : "\r\n";
            idx++;
        }
    }
    _handshakeStrings[idx] = "\r\n";

    _advanceHandshakeTX();
}

WebsocketFilter::~WebsocketFilter()
{
    if (_handshakeStrings != NULL) {
        delete[] _handshakeStrings;
        _handshakeStrings = NULL;
        _num_handshakeStrings = 0;
    }

    if (_rx_close_reason != NULL) delete[] _rx_close_reason;
    _rx_close_reason = NULL;

    if (_pongData != NULL) delete[] _pongData;
    _pongData = NULL;

    delete[] _rxbuf;
    delete[] _txbuf;
}

void WebsocketFilter::_advanceHandshakeTX(void)
{
    if (_handshakeStrings == NULL) return;
    if (_state != HANDSHAKE_TX) return;

#define HS_STRIDX (_sent_handshake[0])
#define HS_STROFF (_sent_handshake[1])
#define HS_STR (_handshakeStrings[_sent_handshake[0]])

    // Copy as many of the remaining strings of handshake into output buffer as
    // possible, until running out of space or end of strings.
    while (_txused < _txbufsiz && HS_STRIDX < _num_handshakeStrings) {
        auto len = strlen(HS_STR);
        auto len_copy = len - HS_STROFF;
        auto txavail = _txbufsiz - _txused;
        if (len_copy > txavail) len_copy = txavail;
        memcpy(_txbuf + _txused, HS_STR + HS_STROFF, len_copy);
        _txused += len_copy;
        HS_STROFF += len_copy;

        // Go to next string of handshake
        if (HS_STROFF >= len) {
            HS_STRIDX++;
            HS_STROFF = 0;
        }
    }

    if (HS_STRIDX >= _num_handshakeStrings) {
        // End of handshake queued, switching to handshake response
        _state = HANDSHAKE_RX;
        delete[] _handshakeStrings;
        _handshakeStrings = NULL;
        _num_handshakeStrings = 0;
    }
}

void WebsocketFilter::_discardTxData(size_t n_bytes)
{
    if (n_bytes <= 0) return;
    if (n_bytes < _txused) {
        memmove(_txbuf, _txbuf + n_bytes, _txused - n_bytes);
        _txused -= n_bytes;
    } else {
        _txused = 0;
    }
}

void WebsocketFilter::_discardRxData(size_t n_bytes)
{
    if (n_bytes <= 0) return;
    if (n_bytes < _rxused) {
        memmove(_rxbuf, _rxbuf + n_bytes, _rxused - n_bytes);
        _rxused -= n_bytes;
    } else {
        _rxused = 0;
    }
}

void WebsocketFilter::fetchDataPtrForStream(uint8_t * & buffer, size_t & n_bytes)
{
    buffer = _txbuf;
    n_bytes = _txused;
}

void WebsocketFilter::discardFetchedData(size_t n_bytes)
{
    if (n_bytes > _txused) n_bytes = _txused;
    if (n_bytes <= 0) return;

    _discardTxData(n_bytes);

    if (_state == HANDSHAKE_TX) {
        // Some space was freed from buffer, advance handshake
        _advanceHandshakeTX();
    }

    if (_state == WEBSOCKET_OPEN && _pendingPong) {
        // Check if pending pong can be enqueued in freed space
        if (2 + _pongDataLen <= _txbufsiz - _txused) {
            _enqueueOutgoingFrame(WS_OP_CTRL_PONG, _pongDataLen,
                (_pongDataLen > 0) ? _pongData : NULL, false, true);
            if (_pongData != NULL) delete[] _pongData;
            _pongData = NULL;
            _pongDataLen = 0;
            _pendingPong = false;
        }
    }
}

bool WebsocketFilter::fetchDataForStream(size_t max_size, uint8_t * buffer, size_t & n_bytes)
{
    if (max_size <= 0 || buffer == NULL) return false;

    n_bytes = _txused;
    if (n_bytes > max_size) n_bytes = max_size;
    if (n_bytes <= 0) return true;

    memcpy(buffer, _txbuf, n_bytes);
    discardFetchedData(n_bytes);

    return true;
}

size_t WebsocketFilter::addDataFromStream(size_t size, const uint8_t * buffer)
{
    if (size > _rxbufsiz - _rxused) size = _rxbufsiz - _rxused;
    if (size > 0) {
        memcpy(_rxbuf + _rxused, buffer, size);
        _rxused += size;

        if (_state == HANDSHAKE_RX) {
            // New data is available, advance handshake response check
            _runHandshakeResponseCheck();
        }

        if (_state == WEBSOCKET_OPEN && _rxused > 0) {
            // Received frame header or data, analyse and remove frame headers
            _runFrameDataParse();
        }
    }

    return size;
}

void WebsocketFilter::_runHandshakeResponseCheck(void)
{
    // Each iteration requires at least one complete header to be present at
    // the beginning of the rx buffer.
    while (_state == HANDSHAKE_RX && _rxused > 0) {
        log_v("_num_respHdrs = %u _rxused = %u", _num_respHdrs, _rxused);
        // A null character is not allowed anywhere in the response. Finding one
        // means the handshake failed (maybe stream is not plaintext HTTP?)
        if (NULL != memchr(_rxbuf, '\0', _rxused)) {
            log_w("NULL found in handshake response (non-HTTP stream?)");
            _state = HANDSHAKE_ERR;
            _err = HANDSHAKE_FAILED;
            break;
        }

        // A complete header must terminate in \r\n. No solitary \r is allowed.
        uint8_t * cr = (uint8_t *)memchr(_rxbuf, '\r', _rxused);
        if (cr == NULL) {
            // \r was not found. Maybe we need to wait for more data, unless
            // buffer is full, which means the "header" is overlong, or (again)
            // not an plaintext HTTP response.
            if (_rxused >= _rxbufsiz) {
                log_w("overlong header (1) in handshake response (non-HTTP stream?)");
                _state = HANDSHAKE_ERR;
                _err = HANDSHAKE_FAILED;
            }
            break;
        }
        if ((cr - _rxbuf) == _rxused - 1) {
            // \r found at the very edge of the response. Maybe we need to wait
            // for more data, unless buffer is full, which means the "header"
            // is overlong
            if (_rxused >= _rxbufsiz) {
                log_w("overlong header (2) in handshake response (non-HTTP stream?)");
                _state = HANDSHAKE_ERR;
                _err = HANDSHAKE_FAILED;
            }
            break;
        }

        // Now it is safe to check whether next character is \n
        if (*(cr + 1) != '\n') {
            log_w("solitary CR in handshake response (non-HTTP stream?)");
            _state = HANDSHAKE_ERR;
            _err = HANDSHAKE_FAILED;
            break;
        }

        char * hdr = (char *)_rxbuf;
        size_t hdrlen = (cr - _rxbuf) + 2;  // Length of header including \r\n

        *cr = '\0'; // Overwrite CR with null to treat header as C-string
        log_v("Examining hdr: [%s]", hdr);

        if (_num_respHdrs == 0) {
            // First header MUST be the response:
            // HTTP/1.1 101 Switching Protocols
            if (strstr(hdr, "HTTP/1.1 101 ") != hdr) {
                log_w("invalid HTTP response: %s", hdr);
                _state = HANDSHAKE_ERR;
                _err = HANDSHAKE_FAILED;
                break;
            }
        } else {
            if (strlen(hdr) > 0) {
                // Rest of headers must be of the form
                // KEY: VALUE
                char * k = hdr;
                char * v = strstr(hdr, ": ");
                if (v == NULL) {
                    log_w("invalid HTTP header: %s", hdr);
                    _state = HANDSHAKE_ERR;
                    _err = HANDSHAKE_FAILED;
                    break;
                }
                *v = '\0'; v += 2;
                while (*v == ' ') v++;

                // So... what header do we have here...?
                if (0 == strcasecmp(k, "Upgrade")) {
                    // This header MUST exist and MUST have the value: websocket
                    if (0 != strcasecmp(v, "websocket")) {
                        // Invalid Upgrade header
                        log_w("invalid Upgrade header: %s", v);
                        _state = HANDSHAKE_ERR;
                        _err = HANDSHAKE_FAILED;
                        break;
                    }
                    _hs_upgrade_websocket = true;
                } else if (0 == strcasecmp(k, "Connection")) {
                    // This header MUST exist and MUST have the value: Upgrade
                    if (0 != strcasecmp(v, "Upgrade")) {
                        // Invalid Upgrade header
                        log_w("invalid Connection header: %s", v);
                        _state = HANDSHAKE_ERR;
                        _err = HANDSHAKE_FAILED;
                        break;
                    }
                    _hs_connection_upgrade = true;
                } else if (0 == strcasecmp(k, "Sec-WebSocket-Accept")) {
                    // This header MUST exist, and its value MUST match the
                    // resulting base64 string for the SHA1 header.
                    char wskey[61];
                    strcpy(wskey, _base64_key);
                    strcat(wskey, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

                    unsigned char sha1_output[20];
                    mbedtls_sha1_ret((unsigned char *)wskey, 60, sha1_output);

                    char expected_base64[29]; size_t dummy_olen;
                    memset(expected_base64, 0, sizeof(expected_base64));
                    mbedtls_base64_encode(
                        (unsigned char *)expected_base64, sizeof(expected_base64),
                        &dummy_olen,
                        (unsigned char *)sha1_output, sizeof(sha1_output));

                    if (0 != strcmp(v, expected_base64)) {
                        // Invalid Sec-WebSocket-Accept
                        log_w("invalid Sec-WebSocket-Accept: expected %s found %s", expected_base64, v);
                        _state = HANDSHAKE_ERR;
                        _err = HANDSHAKE_FAILED;
                        break;
                    }
                    _hs_sec_websocket_accept = true;
                } else  if (0 == strcasecmp(k, "Sec-WebSocket-Protocol")) {
                    // Ignore protocol for now...
                    log_d("(ignored protocol) %s: %s", k, v);
                } else {
                    log_v("(ignored header) %s: %s", k, v);
                }
            } else {
                // Reached end of response. All checks must have passed
                if (!_hs_upgrade_websocket) {
                    log_w("missing header Upgrade: websocket");
                    _state = HANDSHAKE_ERR;
                    _err = HANDSHAKE_FAILED;
                } else if (!_hs_connection_upgrade) {
                    log_w("missing header Connection: Upgrade");
                    _state = HANDSHAKE_ERR;
                    _err = HANDSHAKE_FAILED;
                } else if (!_hs_sec_websocket_accept) {
                    log_w("missing header Sec-WebSocket-Accept");
                    _state = HANDSHAKE_ERR;
                    _err = HANDSHAKE_FAILED;
                } else {
                    // Success! Packet exchange can start now.
                    _state = WEBSOCKET_OPEN;
                    _rx_inheader = true;
                }
            }
        }

        // Header processed, may be discarded now
        _num_respHdrs++;
        _discardRxData(hdrlen);
    }
}

void WebsocketFilter::_runFrameDataParse()
{
    while (_state == WEBSOCKET_OPEN && _rxused > 0) {

        if (_rx_inheader) {
            // Need to accumulate one complete header to evaluate further
            if (_rxused < 2) return;

            // Check if there is enough data for header
            size_t hdrlength = 2;
            uint8_t mask = _rxbuf[1];
            if ((mask & 0x7f) == 0x7f) {
                hdrlength = 10;
            } else if ((mask & 0x7f) == 0x7e) {
                hdrlength = 4;
            }
            if (_rxused < hdrlength + ((mask & 0x80) ? 4 : 0)) return;

            _rx_opcode = _rxbuf[0];
            _rx_framelen = 0;
            if ((mask & 0x7f) == 0x7f) {
                // Extract 64-bit frame length, network byte order
                for (auto i = 0; i < 8; i++) _rx_framelen = (_rx_framelen << 8) | _rxbuf[i+2];
            } else if ((mask & 0x7f) == 0x7e) {
                // Extract 16-bit frame length, network byte order
                for (auto i = 0; i < 2; i++) _rx_framelen = (_rx_framelen << 8) | _rxbuf[i+2];
            } else {
                // 7-bit frame length
                _rx_framelen = mask & 0x7f;
            }
            log_v("hdrlength=%u frame length=%u", hdrlength, (uint32_t)_rx_framelen);

            // Extract last-frame flag, clear reserved flags (ignored)
            bool lastframe = ((_rx_opcode & 0x80) != 0);
            _rx_opcode &= 0x0F;

            // Use identity mask unless flag set by server (NON-COMPLIANT)
            memset(_rx_maskdata, 0, 4);
            if (mask & 0x80) {
                log_w("mask data not compliant for client recv!");
                memcpy(_rx_maskdata, _rxbuf + hdrlength, 4);
            }
            _rx_maskidx = 0;

            _discardRxData(hdrlength + ((mask & 0x80) ? 4 : 0));
            _rx_inheader = false;

            switch (_rx_opcode) {
            case WS_OP_DATA_TEXT:   // Start of text frame
                log_v("start of text frame");
                _rx_binframe = false;
                _rx_textframe = true;
                if (!_rx_lastframe) {
                    log_e("expecting continuation frame");
                    _state = WEBSOCKET_ERROR;
                    _err = PROTOCOL_ERROR;
                }
                _rx_lastframe = lastframe;
                _rx_packetoffset = 0;
                break;
            case WS_OP_DATA_BINARY: // Start of binary frame
                log_v("start of binary frame");
                _rx_binframe = true;
                _rx_textframe = false;
                if (!_rx_lastframe) {
                    log_e("expecting continuation frame");
                    _state = WEBSOCKET_ERROR;
                    _err = PROTOCOL_ERROR;
                }
                _rx_lastframe = lastframe;
                _rx_packetoffset = 0;
                break;
            case WS_OP_DATA_CONT:   // Continuation frame
                log_v("continuation frame");
                _rx_lastframe = lastframe;
                if (!_rx_textframe && !_rx_binframe) {
                    log_e("continuation frame without start frame!");
                    _state = WEBSOCKET_ERROR;
                    _err = PROTOCOL_ERROR;
                }
                break;
            case WS_OP_CTRL_CLOSE:
            case WS_OP_CTRL_PING:
            case WS_OP_CTRL_PONG:
                log_v("control frame 0x%02x", _rx_opcode);
                if (!lastframe) {
                    log_e("fragmented control frame 0x%02x", _rx_opcode);
                    _state = WEBSOCKET_ERROR;
                    _err = PROTOCOL_ERROR;
                }
                if (_rx_framelen > 125) {
                    log_e("overlong control frame 0x%02x", _rx_opcode);
                    _state = WEBSOCKET_ERROR;
                    _err = PROTOCOL_ERROR;
                }
                break;
            default:
                log_e("unimplemented opcode 0x%02x", _rx_opcode);
                _state = WEBSOCKET_ERROR;
                _err = PROTOCOL_ERROR;
                break;
            }
        }

        if (!_rx_inheader && _state == WEBSOCKET_OPEN) {
            // Some data from payload, picked only if control code. According to
            // RFC 6455, any payload for a control code must be no greater than 125
            // bytes.
            if (_rx_opcode >= WS_OP_CTRL_START && _rxused >= _rx_framelen) {
                switch (_rx_opcode) {
                case WS_OP_CTRL_CLOSE:
                    if (_rx_framelen >= 2) {
                        // 2 byte code, rest is text explanation
                        _rx_close_code = (((uint16_t)_rxbuf[0]) << 8) | _rxbuf[1];
                        if (_rx_framelen > 2) {
                            _rx_close_reason = new char[_rx_framelen - 2 + 1];
                            memcpy(_rx_close_reason, _rxbuf + 2, _rx_framelen - 2);
                            _rx_close_reason[_rx_framelen - 2] = '\0';
                        }
                    }
                    _state = WEBSOCKET_CLOSED;
                    _err = REMOTE_CTRL_CLOSE;
                    break;
                case WS_OP_CTRL_PING:
                    // Check if PONG packet can be immediately enqueued
                    if (2 + _rx_framelen <= _txbufsiz - _txused) {
                        // Enqueue PONG packet immediately
                        _enqueueOutgoingFrame(WS_OP_CTRL_PONG, _rx_framelen,
                            (_rx_framelen > 0) ? _rxbuf : NULL, false, true);
                    } else {
                        // Copy PING payload to be enqueued later
                        _pendingPong = true;
                        if (_pongData != NULL) delete[] _pongData;
                        _pongData = NULL;
                        _pongDataLen = _rx_framelen;
                        if (_pongDataLen > 0) {
                            _pongData = new uint8_t[_pongDataLen];
                            memcpy(_pongData, _rxbuf, _pongDataLen);
                        }
                    }
                    break;
                case WS_OP_CTRL_PONG:
                    // Ignored
                    break;
                }

                if (_rx_framelen > 0) _discardRxData(_rx_framelen);
                _rx_framelen = 0;
                _rx_inheader = true;    // Process next RX frame header
            }
        }

        if (!_rx_inheader) break;
    }
}

bool WebsocketFilter::isPacketDataAvailable(void)
{
    if (_state != WEBSOCKET_OPEN) return false;
    if (_err != NO_ERROR) return false;
    return !_rx_inheader && (_rxused > 0);
}

void WebsocketFilter::fetchPacketData(size_t max_size, uint8_t * buffer,
    size_t & n_bytes, uint64_t & packet_offset, bool & packet_binary,
    bool & last_fetch)
{
    n_bytes = 0;
    packet_offset = _rx_packetoffset;
    packet_binary = true;
    last_fetch = false;

    if (max_size <= 0 || buffer == NULL) return;
    if (_rx_textframe) packet_binary = false;
    if (_rx_binframe) packet_binary = true;

    while (!last_fetch && !_rx_inheader && _state == WEBSOCKET_OPEN && max_size > 0 && _rxused > 0) {
        size_t copylen = max_size;
        if (copylen > _rx_framelen) copylen = _rx_framelen;
        if (copylen > _rxused) copylen = _rxused;

        for (auto i = 0; i < copylen; i++) {
            buffer[i] = _rxbuf[i] ^ _rx_maskdata[_rx_maskidx];
            _rx_maskidx = (_rx_maskidx + 1) & 3;
        }
        buffer += copylen;
        max_size -= copylen;
        n_bytes += copylen;
        _rx_packetoffset += copylen;
        _rx_framelen -= copylen;

        _discardRxData(copylen);

        if (_rx_framelen <= 0) {
            // Copied all data from current frame. Any following data MUST be
            // the header of the next frame.
            _rx_inheader = true;
            if (_rx_lastframe) {
                // Last frame of packet
                last_fetch = true;
                _rx_textframe = false;
                _rx_binframe = false;
            } else {
                // Not the last frame, might be more data to copy
            }

            _runFrameDataParse();
        }
    }
}

bool WebsocketFilter::_enqueueOutgoingFrame(uint8_t opcode, uint32_t len,
    const uint8_t * payload, bool masked, bool lastframe)
{
    if (len > 0 && payload == NULL) return false;

    // Calculate required length for full frame
    uint32_t headerlen = 2;
    if (len > 65535)
        headerlen += 8;
    else if (len > 125)
        headerlen += 2;
    if (masked) headerlen += 4;
    if (headerlen + len > _txbufsiz - _txused) return false;

    uint8_t * hdr = _txbuf + _txused;
    uint8_t * p = hdr + headerlen;

    hdr[0] = opcode; if (lastframe) hdr[0] |= 0x80;
    if (len > 65535) {
        hdr[1] = 127;
        hdr[2] = hdr[3] = hdr[4] = hdr[5] = 0;
        hdr[6] = (len >> 24) & 0xFF;
        hdr[7] = (len >> 16) & 0xFF;
        hdr[8] = (len >> 8) & 0xFF;
        hdr[9] = len & 0xFF;
    } else if (len > 125) {
        hdr[1] = 126;
        hdr[2] = (len >> 8) & 0xFF;
        hdr[3] = len & 0xFF;
    } else {
        hdr[1] = len;
    }
    if (masked) {
        hdr[1] |= 0x80;

        uint8_t * tx_maskdata = p - 4;
        *((uint32_t *)tx_maskdata) = random();
        log_v("mask = 0x%08x", *((uint32_t *)tx_maskdata));

        for (auto i = 0; i < len; i++) p[i] = payload[i] ^ tx_maskdata[i & 3];
    } else {
        if (len > 0) memcpy(p, payload, len);
    }

    _txused += headerlen + len;

    return true;
}

bool WebsocketFilter::startPacket(bool packet_binary)
{
    if (_state != WEBSOCKET_OPEN) return false;
    if (_tx_pktopen) return false;
    uint8_t n_opcode = packet_binary ? WS_OP_DATA_BINARY : WS_OP_DATA_TEXT;
    if (_tx_opcode != WS_OP_DATA_CONT && _tx_opcode != n_opcode) return false;
    _tx_opcode = n_opcode;
    _tx_pktopen = true;
    return true;
}

size_t WebsocketFilter::addPacketData(size_t size, const uint8_t * buffer, bool endOfPacket)
{
    if (_state != WEBSOCKET_OPEN) {
        log_w("invalid websocket state %u", _state);
        return 0;
    }
    if (size == 0 || buffer == NULL) {
        log_e("invalid size or ptr");
        return 0;
    }
    if (!_tx_pktopen) {
        log_w("attempt to add data to not-opened packet!");
        return 0;
    }

    uint8_t fixedhdr = 2 + 4;   // two-byte minimum plus mask data
    size_t txavail = _txbufsiz - _txused;

    if (txavail <= fixedhdr) return 0; // Not enough space for even minimum header

    size_t copylen = txavail - fixedhdr;
    if (copylen > size) copylen = size;

    if (copylen > 65535U) {
        // 8-byte size field required, check if it fits
        if (fixedhdr + 8 + copylen > txavail) {
            copylen = txavail - fixedhdr - 8;
        }
    }
    if (copylen > 125 && copylen <= 65535U) {
        // 2-byte size field required, check if it fits
        if (fixedhdr + 2 + copylen > txavail) {
            copylen = txavail - fixedhdr - 2;
        }
    }

    // Cannot signal end-of-packet unless indicated by parameter AND all
    // supplied data actually fits in available space
    if (endOfPacket && copylen < size) endOfPacket = false;

    if (!_enqueueOutgoingFrame(_tx_opcode, copylen, buffer, true, endOfPacket)) {
        log_e("BUG: incorrect calculation of required space: size=%u copylen=%u txavail=%u",
            size, copylen, txavail);
        return 0;
    }
    _tx_opcode = WS_OP_DATA_CONT;
    if (endOfPacket) _tx_pktopen = false;

    return copylen;
}