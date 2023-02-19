/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include "pn_space.h"
#include "constant.h"

namespace utils {
    namespace fnet::quic::status {

        struct HandshakeStatus {
           private:
            std::uint64_t sent_bytes = 0;
            std::uint64_t recv_bytes = 0;
            enum HandshakeFlag : std::uint16_t {
                flag_none = 0x00,
                flag_is_sever = 0x01,
                flag_has_sent_handshake = 0x02,
                flag_has_received_handshake = 0x04,
                flag_has_received_handshake_ack = 0x08,
                flag_handshake_confirmed = 0x10,
                flag_handshake_complete = 0x20,
                flag_retry_received = 0x40,
            } flag;

            friend constexpr HandshakeFlag& operator|=(HandshakeFlag& f, HandshakeFlag flag) noexcept {
                f = HandshakeFlag(f | flag);
                return f;
            }

           public:
            constexpr void reset(bool local_is_server) {
                flag = flag_none;
                flag |= local_is_server ? flag_is_sever : flag_none;
                sent_bytes = 0;
                recv_bytes = 0;
            }

            constexpr void on_packet_sent(PacketNumberSpace space, std::uint64_t size) {
                sent_bytes += size;
                if (space == PacketNumberSpace::handshake) {
                    flag |= flag_has_sent_handshake;
                }
            }

            constexpr void on_packet_received(PacketNumberSpace space, std::uint64_t size) {
                recv_bytes += size;
                if (space == PacketNumberSpace::handshake) {
                    flag |= flag_has_received_handshake;
                }
            }

            constexpr void on_ack_received(PacketNumberSpace space) {
                if (space == PacketNumberSpace::handshake) {
                    flag |= flag_has_received_handshake_ack;
                }
            }

            constexpr void on_handshake_confirmed() {
                flag |= flag_handshake_confirmed;
            }

            constexpr void on_handshake_complete() {
                flag |= flag_handshake_complete;
            }

            constexpr void on_retry_received() {
                flag |= flag_retry_received;
            }

            constexpr bool is_server() const {
                return flag & flag_is_sever;
            }

            constexpr bool handshake_packet_is_sent() const {
                return flag & flag_has_sent_handshake;
            }

            constexpr bool handshake_packet_is_received() const {
                return flag & flag_has_received_handshake;
            }

            constexpr bool handshake_packet_ack_is_received() const {
                return flag & flag_has_received_handshake_ack;
            }

            constexpr bool has_received_retry() const {
                return flag & flag_retry_received;
            }

            constexpr bool handshake_confirmed() const {
                return flag & flag_handshake_confirmed;
            }

            constexpr bool handshake_complete() const {
                return flag & flag_handshake_complete;
            }

            constexpr bool is_at_anti_amplification_limit() const {
                if (peer_address_validated()) {
                    return false;
                }
                return sent_bytes >= amplification_factor * recv_bytes;
            }

            constexpr bool peer_address_validated() const {
                if (!is_server()) {
                    return true;  // server address is always validated
                }
                return handshake_packet_is_received();
            }

            constexpr bool peer_completed_address_validation() const {
                if (is_server()) {
                    return true;
                }
                return handshake_packet_ack_is_received() || handshake_confirmed();
            }
        };

    }  // namespace fnet::quic::status
}  // namespace utils
