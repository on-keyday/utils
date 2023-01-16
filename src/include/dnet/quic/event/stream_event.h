/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// stream_event - event
#pragma once
#include "event.h"
#include "../stream/impl/stream.h"

namespace utils {
    namespace dnet::quic::event {
        template <class Lock>
        inline Status send_stream(PacketType, packetnum::Value pn, ACKWaitVec& vec, io::writer& w, priority& prio, std::shared_ptr<void>& arg) {
            namespace impl = stream::impl;
            auto ptr = static_cast<impl::SendUniStream<Lock>*>(arg.get());
            auto final_state = ptr->detect_ack();
            if (final_state == stream::SendState::data_recved) {
                return Status::discard;
            }
            auto res = ptr->send(pn, vec, w);
            if (res == IOResult::invalid_data ||
                res == IOResult::fatal) {
                return Status::fatal;
            }
            if (res == IOResult::not_in_io_state) {
                return Status::discard;
            }
            return Status::reorder;
        }

        template <class Lock>
        inline Status send_reset(PacketType, packetnum::Value pn, ACKWaitVec& vec, io::writer& w, priority& prio, std::shared_ptr<void>& arg) {
            namespace impl = stream::impl;
            auto ptr = static_cast<impl::SendUniStream<Lock>*>(arg.get());
            auto final_state = ptr->detect_ack();
            if (final_state == stream::SendState::reset_recved) {
                return Status::discard;
            }
            auto [res, state] = ptr->reset(pn, vec, w);
            if (res == IOResult::invalid_data ||
                res == IOResult::fatal) {
                return Status::fatal;
            }
            if (res == IOResult::not_in_io_state) {
                return Status::discard;
            }
            return Status::reorder;
        }

    }  // namespace dnet::quic::event
}  // namespace utils
