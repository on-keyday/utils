/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// ack_event - ack event
#pragma once
#include "event.h"
#include "../frame/writer.h"
#include "../frame/cast.h"
#include "../ack/unacked.h"
#include "../ack/sent_history.h"

namespace utils {
    namespace dnet::quic::event {
        inline std::pair<Status, error::Error> send_ack(const packet::PacketSummary& packet, ACKWaitVec&, frame::fwriter& w, std::shared_ptr<void>& arg) {
            auto ackh = static_cast<ack::UnackedPackets*>(arg.get());
            ack::ACKRangeVec* ackvec = ackh->generate_ragnes_from_received(status::from_packet_type(packet.type));
            if (!ackvec) {
                return {Status::reorder, error::none};
            }
            auto [ack, ok] = frame::convert_to_ACKFrame<slib::vector>(*ackvec);
            if (!ok) {
                return {Status::fatal, error::Error("generate ack range failed. library bug!!")};
            }
            if (w.remain().size() < ack.len()) {
                return {Status::reorder, error::none};
            }
            if (!w.write(ack)) {
                return {Status::fatal, error::Error("failed to render ACKFrame")};
            }
            ackh->clear(status::from_packet_type(packet.type));
            return {Status::reorder, error::none};
        }

        inline std::pair<Status, error::Error> recv_ack(const packet::PacketSummary& packet, frame::Frame& f, std::shared_ptr<void>& arg) {
            auto ackh = static_cast<ack::SentPacketHistory*>(arg.get());
            if (auto ack = frame::cast<frame::ACKFrame<slib::vector>>(&f)) {
                ack::RemovedPackets acked;
                ackh->on_ack_received(status::from_packet_type(packet.type), acked, nullptr, *ack, );
                if (auto err = ackh->on_ack_received(*ack, ack::from_packet_type(packet.type))) {
                    return {Status::fatal, err};
                }
            }
            return {Status::reorder, error::none};
        }

    }  // namespace dnet::quic::event
}  // namespace utils
