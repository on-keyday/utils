/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../error.h"
#include "../status/status.h"
#include "sent_packet.h"

namespace utils {
    namespace fnet::quic::ack {

        template <class Impl>
        struct SendHistoryInterface {
           private:
            Impl impl;

           public:
            constexpr void reset() {
                impl.reset();
            }

            template <class Alg>
            constexpr error::Error on_packet_sent(status::Status<Alg>& status, status::PacketNumberSpace space, SentPacket&& sent) {
                return impl.on_packet_sent(status, space, std::move(sent));
            }

            template <class Alg>
            error::Error on_ack_received(status::Status<Alg>& status,
                                         status::PacketNumberSpace space,
                                         RemovedPackets& acked, RemovedPackets* lost,
                                         const frame::ACKFrame<slib::vector>& ack,
                                         auto&& is_flow_control_limited) {
                return impl.on_ack_received(status, space, acked, lost, ack, is_flow_control_limited);
            }

            template <class Alg>
            error::Error maybe_loss_detection_timeout(status::Status<Alg>& status, RemovedPackets* lost) {
                return impl.maybe_loss_detection_timeout(status, lost);
            }

            template <class Alg>
            void on_packet_number_space_discarded(status::Status<Alg>& status, status::PacketNumberSpace space) {
                impl.on_packet_number_space_discarded(status, space);
            }

            template <class Alg>
            void on_retry_received(status::Status<Alg>& status) {
                impl.on_retry_received(status);
            }
        };
    }  // namespace fnet::quic::ack
}  // namespace utils
