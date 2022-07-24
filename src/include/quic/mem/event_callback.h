/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// callbacks - user defined callback
#pragma once
#include "../doc.h"
#include "../packet/packet_head.h"
#include "../core/event.h"
#include "callback.h"

namespace utils {
    namespace quic {
        namespace callback {
            template <class... Args>
            using EventCB = mem::CB<mem::nocontext_t, void, Args...>;
            struct GlobalCallbacks {
                EventCB<tsize> on_enter_progress;
                EventCB<core::EventArg> on_event;
            };

            struct CommonCallbacks {
                CommonCallbacks* parent;
                EventCB<const byte*, tsize, tsize, packet::FirstByte, uint, bool> on_discard;
                EventCB<packet::Error, const char*, packet::Packet*> on_packet_error;
            };
#define DISPATCH_CALLBACK(common, event_cb, ...) \
    {                                            \
        auto p = common;                         \
        while (p) {                              \
            if (p->event_cb) {                   \
                p->event_cb(__VA_ARGS__);        \
            }                                    \
            p = p->parent;                       \
        }                                        \
    }
        }  // namespace callback
    }      // namespace quic
}  // namespace utils
