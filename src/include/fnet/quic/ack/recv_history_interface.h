/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../status/pn_space.h"
#include "../packet_number.h"
#include "../status/config.h"
#include "../frame/writer.h"
#include "../ioresult.h"

namespace futils {
    namespace fnet::quic::ack {

        template <class Impl>
        struct RecvHistoryInterface {
           private:
            Impl impl;

           public:
            constexpr void reset() {
                impl.reset();
            }

            constexpr bool is_duplicated(status::PacketNumberSpace space, packetnum::Value pn) const {
                return impl.is_duplicated(space, pn);
            }

            constexpr void on_packet_number_space_discarded(status::PacketNumberSpace space) {
                impl.on_packet_number_space_discarded(space);
            }

            constexpr void delete_onertt_under(packetnum::Value pn) {
                impl.delete_onertt_under(pn);
            }

            constexpr void on_packet_processed(
                status::PacketNumberSpace space, packetnum::Value pn,
                bool is_ack_eliciting,
                const status::InternalConfig& config) {
                impl.on_packet_processed(space, pn, is_ack_eliciting, config);
            }

            constexpr IOResult send(frame::fwriter& w, status::PacketNumberSpace space, const status::InternalConfig& config, packetnum::Value& largest_ack) {
                return impl.send(w, space, config, largest_ack);
            }

            constexpr time::Time get_deadline() const {
                return impl.get_deadline();
            }
        };
    }  // namespace fnet::quic::ack
}  // namespace futils
