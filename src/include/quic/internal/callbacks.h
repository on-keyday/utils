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

namespace utils {
    namespace quic {
        namespace callback {
            struct GlobalCallbacks {
                void* C;
                void (*on_enter_progress_f)(void* C, tsize seq);
                void on_enter_progress(tsize seq) {
                    if (on_enter_progress_f) {
                        on_enter_progress_f(C, seq);
                    }
                }
            };

            struct CommonCallbacks {
                CommonCallbacks* parent;
                void* C;
                void (*on_discard_f)(void* C, const byte* data, tsize size, tsize offset, packet::FirstByte fb, uint version, bool versuc);
                void (*on_packet_error_f)(void* C, const byte* data, tsize size, tsize offset, packet::FirstByte fb, uint version, packet::Error err, const char* where);

                void on_packet_error(const byte* data, tsize size, tsize offset, packet::FirstByte fb, uint version, packet::Error err, const char* where) {
                    if (on_packet_error_f) {
                        on_packet_error_f(C, data, size, offset, fb, version, err, where);
                    }
                    if (parent) {
                        parent->on_packet_error(data, size, offset, fb, version, err, where);
                    }
                }

                void on_discard(const byte* data, tsize size, tsize offset, packet::FirstByte fb, uint version, bool versuc) {
                    if (on_discard_f) {
                        on_discard_f(C, data, size, offset, fb, version, versuc);
                    }
                    if (parent) {
                        parent->on_discard(data, size, offset, fb, version, versuc);
                    }
                }
            };
        }  // namespace callback
    }      // namespace quic
}  // namespace utils
