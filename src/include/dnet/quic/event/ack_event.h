/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// ack_event - ack event
#pragma once
#include "../ack/loss_detection.h"
#include "event.h"
#include "../frame/writer.h"
#include "../frame/cast.h"
#include "../ack/unacked.h"

namespace utils {
    namespace dnet::quic::event {
        inline std::pair<Status, error::Error> send_ack(const packet::PacketSummary& packet, ACKWaitVec&, frame::fwriter& w, priority& prio, std::shared_ptr<void>& arg) {
            auto ackh = static_cast<ack::UnackedPacket*>(arg.get());
            ack::ACKRangeVec ackvec;
            if (!ackh->gen_ragnes_from_recved(ack::from_packet_type(packet.type), ackvec)) {
                return {Status::reorder, error::none};
            }
            auto [ack, ok] = frame::convert_to_ACKFrame<slib::vector>(ackvec);
            if (!ok) {
                return {Status::fatal, error::Error("generate ack range failed. library bug!!")};
            }
            if (w.remain().size() < ack.len()) {
                return {Status::reorder, error::none};
            }
            if (!w.write(ack)) {
                return {Status::fatal, error::Error("failed to render ACKFrame")};
            }
            ackh->clear(ack::from_packet_type(packet.type));
            return {Status::reorder, error::none};
        }

        inline std::pair<Status, error::Error> recv_ack(const packet::PacketSummary& packet, frame::Frame& f, std::shared_ptr<void>& arg) {
            auto ackh = static_cast<ack::LossDetectionHandler*>(arg.get());
            if (auto ack = frame::cast<frame::ACKFrame<slib::vector>>(&f)) {
                if (auto err = ackh->on_ack_received(*ack, ack::from_packet_type(packet.type))) {
                    return {Status::fatal, err};
                }
            }
            return {Status::reorder, error::none};
        }
    }  // namespace dnet::quic::event
}  // namespace utils
