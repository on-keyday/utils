/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// conn_id_event - connection id management event
#pragma once
#include "event.h"
#include "../connid/id_manage.h"
#include "../frame/writer.h"
#include "../frame/cast.h"

namespace utils {
    namespace dnet::quic::event {
        inline std::pair<Status, error::Error> send_conn_id(const packet::PacketSummary& packet, ACKWaitVec& vec, frame::fwriter& w, std::shared_ptr<void>& arg) {
            if (packet.type != PacketType::OneRTT) {
                return {Status::reorder, error::none};
            }
            auto connIDs = static_cast<connid::IDManager*>(arg.get());
            if (connIDs->send_retire_connection_id(vec, w) == IOResult::fatal) {
                return {Status::fatal, error::Error("failed to send RetireConnectionID frame")};
            }
            if (connIDs->send_new_connection_id(vec, w) == IOResult::fatal) {
                return {Status::fatal, error::Error("failed to send NewConnectionID frame")};
            }
            return {Status::reorder, error::none};
        }

        inline std::pair<Status, error::Error> recv_conn_id(const packet::PacketSummary&, frame::Frame& f, std::shared_ptr<void>& arg) {
            auto connIDs = static_cast<connid::IDManager*>(arg.get());
            if (auto r = frame::cast<frame::RetireConnectionIDFrame>(&f)) {
                connIDs->recv_retire_connection_id(*r);
            }
            if (auto n = frame::cast<frame::NewConnectionIDFrame>(&f)) {
                if (auto err = connIDs->recv_new_connection_id(*n)) {
                    return {Status::fatal, err};
                }
            }
            return {Status::reorder, error::none};
        }
    }  // namespace dnet::quic::event
}  // namespace utils
