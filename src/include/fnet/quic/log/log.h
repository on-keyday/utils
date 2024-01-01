/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
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
#include "../status/pn_space.h"
#include "../status/loss_timer.h"

namespace futils {
    namespace fnet::quic::log {
        struct ConnLogCallbacks {
            void (*drop_packet)(std::shared_ptr<void>&, PacketType, packetnum::Value, error::Error, view::rvec raw_packet, bool is_decrypted) = nullptr;
            void (*debug)(std::shared_ptr<void>&, const char*) = nullptr;
            void (*sending_packet)(std::shared_ptr<void>&, packet::PacketSummary, view::rvec payload, bool) = nullptr;
            void (*recv_packet)(std::shared_ptr<void>&, packet::PacketSummary, view::rvec payload, bool) = nullptr;
            void (*pto_fire)(std::shared_ptr<void>&, status::PacketNumberSpace) = nullptr;
            void (*loss_timer_state)(std::shared_ptr<void>&, const status::LossTimer&, time::Time now) = nullptr;
            void (*mtu_probe)(std::shared_ptr<void>&, std::uint64_t probe) = nullptr;
            void (*rtt_state)(std::shared_ptr<void>&, const status::RTT& rtt, time::Time now) = nullptr;
        };

        struct ConnLogger {
            std::shared_ptr<void> ctx;
            const ConnLogCallbacks* callbacks = nullptr;

            void drop_packet(PacketType type, packetnum::Value val, error::Error err, view::rvec raw_paket, bool decrypted) {
                if (callbacks && callbacks->drop_packet) {
                    callbacks->drop_packet(ctx, type, val, err, raw_paket, decrypted);
                }
            }

            void debug(const char* msg) {
                if (callbacks && callbacks->debug) {
                    callbacks->debug(ctx, msg);
                }
            }

            void sending_packet(packet::PacketSummary s, view::rvec payload) {
                if (callbacks && callbacks->sending_packet) {
                    callbacks->sending_packet(ctx, s, payload, true);
                }
            }

            void recv_packet(packet::PacketSummary s, view::rvec payload) {
                if (callbacks && callbacks->recv_packet) {
                    callbacks->recv_packet(ctx, s, payload, false);
                }
            }

            void pto_fire(status::PacketNumberSpace space) {
                if (callbacks && callbacks->pto_fire) {
                    callbacks->pto_fire(ctx, space);
                }
            }

            void loss_timer_state(const status::LossTimer& loss, time::Time now) {
                if (callbacks && callbacks->loss_timer_state) {
                    callbacks->loss_timer_state(ctx, loss, now);
                }
            }

            void rtt_state(const status::RTT& rtt, time::Time now) {
                if (callbacks && callbacks->rtt_state) {
                    callbacks->rtt_state(ctx, rtt, now);
                }
            }

            void mtu_probe(std::uint64_t packet_size) {
                if (callbacks && callbacks->mtu_probe) {
                    callbacks->mtu_probe(ctx, packet_size);
                }
            }
        };
    }  // namespace fnet::quic::log
}  // namespace futils
