/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// h2frame_c - http2 frame in c language
#pragma once
#include "../dll/dllh.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    data = 0x0,
    header = 0x1,
    priority = 0x2,
    rst_stream = 0x3,
    settings = 0x4,
    push_promise = 0x5,
    ping = 0x6,
    goaway = 0x7,
    window_update = 0x8,
    continuous = 0x9,
    invalid = 0xff,
} H2FreameType;

typedef enum {
    none = 0x0,
    ack = 0x1,
    end_stream = 0x1,
    end_headers = 0x4,
    padded = 0x8,
    priorityf = 0x20,
} H2Flag;

typedef struct {
    int len;
    uint8_t type;
    uint8_t flag;
    int32_t id;
} H2Frame;

typedef struct {
    uint32_t depend;
    uint8_t weight;
} H2Priority;

typedef struct {
    H2Frame head;
    uint8_t padding;
    const char* data;
    uint32_t datalen;  // extension field
    const char* pad;
} H2DataFrame;

typedef struct {
    H2Frame head;
    uint8_t padding;
    H2Priority priority;
    const char* data;
    uint32_t datalen;  // extension field
    const char* pad;
} H2HeaderFrame;

typedef struct {
    H2Frame head;
    H2Priority priority;
} H2PriorityFrame;

typedef struct {
    H2Frame head;
    uint32_t code;
} H2RstStreamFrame;

typedef struct {
    H2Frame head;
    const char* settings;
} H2SettingsFrame;

typedef struct {
    H2Frame head;
    uint8_t padding;
    int32_t promise;
    const char* data;
    uint32_t datalen;  // extension field
    const char* pad;
} H2PushPromiseFrame;

typedef struct {
    H2Frame head;
    uint64_t opaque;
} H2PingFrame;

typedef struct {
    H2Frame head;
    int32_t processed_id;
    uint32_t code;
    const char* debug_data;
    size_t dbglen;  // extension field
} H2GoAwayFrame;

typedef struct {
    H2Frame head;
    int32_t increment;
} H2WindowUpdateFrame;

typedef struct {
    H2Frame head;
    const char* data;
} H2ContinuationFrame;

typedef struct {
    int err;
    int h2err;
    int strm;
    const char* debug;
} H2ErrCode;

using H2ReadCallback = int (*)(void* user, H2Frame* frame, H2ErrCode* err);

fnet_dll_export(int) parse_h2frame(const char* text, size_t size, size_t* red, H2ErrCode* err, H2ReadCallback cb, void* c);
fnet_dll_export(int) render_h2frame(char* text, size_t* size, size_t cap, H2ErrCode* err, H2Frame* frame, bool auto_correct);
fnet_dll_export(int) pass_h2frame(const char* text, size_t size, size_t* red);
#define INIT(Struct) fnet_dll_export(void) init_##Struct(H2##Struct* ptr);
INIT(Frame)
INIT(DataFrame)
INIT(HeaderFrame)
INIT(PriorityFrame)
INIT(RstStreamFrame)
INIT(SettingsFrame)
INIT(PushPromiseFrame)
INIT(PingFrame)
INIT(GoAwayFrame)
INIT(WindowUpdateFrame)
INIT(ContinuationFrame)
#undef INIT
#ifdef __cplusplus
}
#endif
