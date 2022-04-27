#pragma once

#include <stdint.h>

namespace AsyncMqttClientInternals {

typedef enum {
    NO_ERROR = 0,
    HANDSHAKE_FAILED,   // Websocket handshake failed
    REMOTE_CTRL_CLOSE,  // Remote side sent a close packet
    PROTOCOL_ERROR      // Invalid condition encountered in received frame
} WebsocketError;

typedef enum {
    HANDSHAKE_TX,       // Client is sending HTTP handshake
    HANDSHAKE_RX,       // Client is receiving HTTP response to handshake
    HANDSHAKE_ERR,      // Handshake failed at some point

    WEBSOCKET_OPEN,     // Handshake succeeded, websocket is sending/receiving packets
    WEBSOCKET_CLOSED,   // Received close packet from remote side
    WEBSOCKET_ERROR     // Websocket protocol error
} WebsocketState;

class WebsocketFilter
{
private:
    WebsocketState _state;  // Current state of websocket
    WebsocketError _err;    // Current error status

    uint8_t * _txbuf; size_t _txbufsiz; size_t _txused;
    uint8_t * _rxbuf; size_t _rxbufsiz; size_t _rxused;

    // List of HTTP strings for handshake
    const char ** _handshakeStrings;
    size_t _num_handshakeStrings;
    size_t _sent_handshake[2];  // index,offset of sent handshake

    // base64 representation of 16-bit random value (24 bytes) plus null
    // for Sec-WebSocket-Key header.
    char _base64_key[25];

    // Variables for response checks
    unsigned int _num_respHdrs;
    bool _hs_upgrade_websocket;
    bool _hs_connection_upgrade;
    bool _hs_sec_websocket_accept;

    // RX: are we expecting a frame header?
    bool _rx_inheader;
    bool _rx_textframe;
    bool _rx_binframe;
    bool _rx_lastframe;
    uint8_t _rx_opcode;
    uint64_t _rx_framelen;      // Length of payload portion of frame, without headers/mask
    uint64_t _rx_packetoffset;
    uint8_t _rx_maskdata[4];
    uint8_t _rx_maskidx;

    // Close reason handling
    uint16_t _rx_close_code;
    char * _rx_close_reason;

    // Pending PONG handling
    bool _pendingPong;
    uint8_t * _pongData;
    uint8_t _pongDataLen;

    uint8_t _tx_opcode;
    bool _tx_pktopen;

    void _discardTxData(size_t);
    void _discardRxData(size_t);
    void _advanceHandshakeTX(void);
    void _runHandshakeResponseCheck(void);

    void _runFrameDataParse(void);
    bool _enqueueOutgoingFrame(uint8_t opcode, uint32_t len, const uint8_t * payload,
        bool masked, bool lastframe);
public:
    WebsocketFilter(const char * hostname, const char * wsurl = "/",
        uint32_t n_protos = 0, const char * protos[] = NULL,
        size_t rxbufsiz = 256, size_t txbufsiz = 256);
    ~WebsocketFilter();

    WebsocketState getState(void) { return _state; }
    WebsocketError getError(void) { return _err; }

    uint16_t getCloseCode(void) { return _rx_close_code; }
    const char * getCloseReason(void) { return _rx_close_reason; }

    // Push raw bytes received from a byte stream into websocket RX parser.
    // Returns number of bytes actually consumed from buffer, which might be
    // zero if the internal buffer is full.
    size_t addDataFromStream(size_t size, const uint8_t * buffer);

    // Query number of pending bytes to be sent to stream
    size_t pendingStreamOutputLen(void) { return _txused; }

    // Fetch raw bytes to be sent to byte stream from websocket TX. This
    // places the number of bytes actually placed into the buffer into n_bytes,
    // up to the limit of max_size. After fetching the outgoing data, this data
    // is discarded and the space reused for further processing. Returns true
    // for success (including n_bytes <- 0 for empty buffer), or false for
    // failure (usually a pending error).
    bool fetchDataForStream(size_t max_size, uint8_t * buffer, size_t & n_bytes);

    // Fetch a pointer to the internal buffer of raw bytes queued for output.
    // Unlike fetchDataForStream(), this method will skip the copy to the
    // externally-supplied buffer. However, the app must now remember to call
    // discardFetchedData() with the total amount of bytes that were successfully
    // written to the output stream. After calling this function, the buffer
    // is guaranteed to be valid and unchanged up to n_bytes, until the call to
    // discardFetchedData().
    void fetchDataPtrForStream(uint8_t * & buffer, size_t & n_bytes);

    // Discard data that was written to output stream. Companion to fetchDataPtrForStream().
    void discardFetchedData(size_t n_bytes);


    // Check if some INCOMING frame data is available to be fetched
    bool isPacketDataAvailable(void);

    // Fetch some of the current available packet data:
    // - max_bytes, buffer: (INPUT) size and location of output buffer
    // - n_bytes: number of bytes actually fetched, up to max_bytes
    // - packet_offset: offset from start of packet of fetched data
    // - packet_binary: true if packet is binary, false if text
    // - last_fetch: true if fetch served last of packet data
    //
    // This method will merge together multiple frames belonging to the same
    // fragmented packet, if applicable.
    // After calling this function, n_bytes of packet data are discarded.
    void fetchPacketData(size_t max_size, uint8_t * buffer, size_t & n_bytes,
        uint64_t & packet_offset, bool & packet_binary, bool & last_fetch);



    // Start an outgoing packet, with type of either text or binary
    bool startPacket(bool packet_binary);

    bool isOpenPacket(void) { return _tx_pktopen; }

    // Add data to the outgoing packet started by startPacket(). This returns
    // the number of bytes actually consumed from the buffer and made into a
    // websocket frame. The app SHOULD call fetchDataForStream() at least once
    // after calling this function, in order to fetch resulting packet data and
    // free space for further processing. The endOfPacket flag MUST be set
    // if the buffer contains the last byte of data to be sent to the stream.
    // NOTE: if the endOfPacket flag is set, BUT the method returns less than
    // the specified buffer size, the app MUST fetch some data to be sent to
    // the stream using fetchDataForStream() in order to free some space, then
    // retry with the remaining unsent buffer, and endOfPacket again set.
    size_t addPacketData(size_t size, const uint8_t * buffer, bool endOfPacket);
};

}  // namespace AsyncMqttClientInternals
