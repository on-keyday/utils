/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// capi - c language api
#pragma once
#include "../common/dll_h.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#define NOARG
#else
#define NOARG void
#endif
using byte = unsigned char;

// QUIC api context
typedef struct QUIC QUIC;

Dll(QUIC*) default_QUIC(NOARG);

typedef struct QUICFrame QUICFrame;

enum QUICFrameType {
    PADDING,               // section 19.1
    PING,                  // section 19.2
    ACK,                   // section 19.3
    ACK_ECN,               // with ECN count
    RESET_STREAM,          // section 19.4
    STOP_SENDING,          // section 19.5
    CRYPTO,                // section 19.6
    NEW_TOKEN,             // section 19.7
    STREAM,                // section 19.8
    MAX_DATA = 0x10,       // section 19.9
    MAX_STREAM_DATA,       // section 19.10
    MAX_STREAMS_BIDI,      // section 19.11
    MAX_STREAMS_UNI,       // section 19.11
    DATA_BLOCKED,          // section 19.12
    STREAM_DATA_BLOCKED,   // section 19.13
    STREAMS_BLOCKED_BIDI,  // section 19.14
    STREAMS_BLOCKED_UNI,   // section 19.14
    NEW_CONNECTION_ID,     // section 19.15
    RETIRE_CONNECTION_ID,  // seciton 19.16
    PATH_CHALLAGE,         // seciton 19.17
    PATH_RESPONSE,         // seciton 19.18
    CONNECTION_CLOSE,      // seciton 19.19
    CONNECTION_CLOSE_APP,  // application reason section 19.19
    HANDSHAKE_DONE,        // seciton 19.20
    DATAGRAM = 0x30,       // rfc 9221 datagram
    DATAGRAM_LEN,
};

Dll(QUICFrameType) get_frame_type(QUICFrame* frame);

Dll(const byte*) get_frame_data(QUICFrame* frame, const char* param, size_t* len);

#ifdef __cplusplus
}
#endif
