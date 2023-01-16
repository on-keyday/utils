/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../packet_number.h"
#include "../types.h"
#include <memory>

namespace utils {
    namespace dnet::quic::log {
        struct LogCallbacks {
            void (*drop_packet_cb)(std::shared_ptr<void>&, PacketType, packetnum::Value, error::Error);
        };

        struct Logger {
            std::shared_ptr<void> ctx;
            const LogCallbacks* callbacks = nullptr;

            void drop_packet(PacketType type, packetnum::Value val, error::Error err) {
                if (callbacks && callbacks->drop_packet_cb) {
                    callbacks->drop_packet_cb(ctx, type, val, err);
                }
            }
        };
    }  // namespace dnet::quic::log
}  // namespace utils
