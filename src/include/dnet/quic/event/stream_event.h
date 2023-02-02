/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// stream_event - event
#pragma once
#include "event.h"
#include "../stream/impl/stream.h"
#include "../stream/impl/conn.h"

namespace utils {
    namespace dnet::quic::event {
        template <class Lock>
        inline std::pair<Status, error::Error> send_streams(const packet::PacketSummary& packet, ACKWaitVec& wait, frame::fwriter& w, std::shared_ptr<void>& arg) {
            if (packet.type != PacketType::OneRTT &&
                packet.type != PacketType::ZeroRTT) {
                return {Status::reorder, error::none};
            }
            namespace impl = stream::impl;
            impl::Conn<Lock>* ptr = static_cast<impl::Conn<Lock>*>(arg.get());
            auto res = ptr->send(w, wait);
            if (res == IOResult::invalid_data ||
                res == IOResult::fatal) {
                return {Status::fatal, error::none};
            }
            return {Status::reorder, error::none};
        }

        template <class Lock>
        inline std::pair<Status, error::Error> recv_streams(const packet::PacketSummary& packet, frame::Frame& f, std::shared_ptr<void>& arg) {
            namespace impl = stream::impl;
            impl::Conn<Lock>* conn = static_cast<impl::Conn<Lock>*>(arg.get());
            auto [_, err] = conn->recv(f);
            if (err) {
                return {Status::fatal, std::move(err)};
            }
            return {Status::reorder, error::none};
        }

    }  // namespace dnet::quic::event
}  // namespace utils
