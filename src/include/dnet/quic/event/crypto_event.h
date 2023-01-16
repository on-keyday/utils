/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// crypto_event
#pragma once
#include "event.h"
#include "../crypto/suite.h"
#include "../frame/cast.h"

namespace utils {
    namespace dnet::quic::event {
        inline std::pair<Status, error::Error> send_crypto(const packet::PacketSummary& packet, ACKWaitVec& vec, frame::fwriter& w, priority& prio, std::shared_ptr<void>& arg) {
            auto suite = static_cast<crypto::CryptoSuite*>(arg.get());
            if (suite->send(packet.type, packet.packet_number, vec, w) == IOResult::fatal) {
                return {Status::fatal, error::none};
            }
            return {Status::reorder, error::none};
        }

        std::pair<Status, error::Error> recv_crypto(const packet::PacketSummary& packet, frame::Frame& f, std::shared_ptr<void>& arg) {
            auto suite = static_cast<crypto::CryptoSuite*>(arg.get());
            if (auto cf = frame::cast<frame::CryptoFrame>(&f)) {
                if (auto err = suite->recv(packet.type, *cf)) {
                    return {Status::fatal, err};
                }
            }
            if (auto hd = frame::cast<frame::HandshakeDoneFrame>(&f)) {
                if (auto err = suite->recv_handshake_done()) {
                    return {Status::fatal, err};
                }
            }
            return {Status::reorder, error::none};
        }
    }  // namespace dnet::quic::event
}  // namespace utils
