/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../packet_number.h"
#include "../types.h"
#include <memory>
#include "../../error.h"
#include "../packet/summary.h"
#include "../frame/base.h"
#include "../ack/pn_space.h"

namespace utils {
    namespace dnet::quic::log {
        struct LogCallbacks {
            void (*drop_packet)(std::shared_ptr<void>&, PacketType, packetnum::Value, error::Error);
            void (*debug)(std::shared_ptr<void>&, const char*);
            void (*sending_packet)(std::shared_ptr<void>&, packet::PacketSummary, view::rvec payload);
            void (*recv_packet)(std::shared_ptr<void>&, packet::PacketSummary, view::rvec payload);
            void (*pto_fire)(std::shared_ptr<void>&, ack::PacketNumberSpace);
        };

        struct Logger {
            std::shared_ptr<void> ctx;
            const LogCallbacks* callbacks = nullptr;

            void drop_packet(PacketType type, packetnum::Value val, error::Error err) {
                if (callbacks && callbacks->drop_packet) {
                    callbacks->drop_packet(ctx, type, val, err);
                }
            }

            void debug(const char* msg) {
                if (callbacks && callbacks->debug) {
                    callbacks->debug(ctx, msg);
                }
            }

            void sending_packet(packet::PacketSummary s, view::rvec payload) {
                if (callbacks && callbacks->sending_packet) {
                    callbacks->sending_packet(ctx, s, payload);
                }
            }

            void recv_packet(packet::PacketSummary s, view::rvec payload) {
                if (callbacks && callbacks->recv_packet) {
                    callbacks->recv_packet(ctx, s, payload);
                }
            }

            void pto_fire(ack::PacketNumberSpace space) {
                if (callbacks && callbacks->pto_fire) {
                    callbacks->pto_fire(ctx, space);
                }
            }
        };
    }  // namespace dnet::quic::log
}  // namespace utils
