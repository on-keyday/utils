/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// conn_flow - connection flow controler implementaion
#pragma once
#include "../core/conn_base.h"
#include "../../resend/ack_handler.h"

namespace utils {
    namespace fnet::quic::stream::impl {

        template <class TConfig>
        struct ConnFlowControl {
            core::ConnectionBase<TConfig> base;
            resend::ACKHandler max_data;
            resend::ACKHandler max_uni_streams;
            resend::ACKHandler max_bidi_streams;

            // don't get lock by yourself
            bool update_max_data(auto&& decide_new_limit) {
                const auto locked = base.accept_uni_lock();
                return base.update_max_data(decide_new_limit);
            }

            // don't get lock by yourself
            bool update_max_uni_streams(auto&& decide_new_limit) {
                const auto locked = base.accept_uni_lock();
                return base.update_max_uni_streams(decide_new_limit);
            }

            // don't get lock by yourself
            bool update_max_bidi_streams(auto&& decide_new_limit) {
                const auto locked = base.accept_bidi_lock();
                return base.update_max_bidi_streams(decide_new_limit);
            }

           private:
            IOResult send_limit(auto&& observer, resend::ACKHandler& rec, auto&& send_updated, auto&& send_force) {
                auto res = send_updated();
                if (res == IOResult::ok) {
                    rec.wait(observer);  // clear old record
                    return res;
                }
                if (rec.not_confirmed()) {
                    if (rec.is_lost()) {
                        if (auto res = send_force(); res == IOResult::fatal) {
                            return res;
                        }
                        else if (res == IOResult::ok) {
                            rec.wait(observer);
                        }
                    }
                    if (rec.is_ack()) {
                        rec.confirm();
                    }
                }
                return IOResult::ok;
            }

           public:
            IOResult send_max_data(frame::fwriter& w, auto&& observer) {
                const auto locked = base.recv_lock();
                return send_limit(
                    observer, max_data,
                    [&] { return base.send_max_data_if_updated(w); },
                    [&] { return base.send_max_data(w); });
            }

            IOResult send_max_uni_streams(frame::fwriter& w, auto&& observer) {
                const auto locked = base.accept_uni_lock();
                return send_limit(
                    observer, max_uni_streams,
                    [&] { return base.send_max_uni_streams_if_updated(w); },
                    [&] { return base.send_max_uni_streams(w); });
            }

            IOResult send_max_bidi_streams(frame::fwriter& w, auto&& observer) {
                const auto locked = base.accept_bidi_lock();
                return send_limit(
                    observer, max_bidi_streams,
                    [&] { return base.send_max_bidi_streams_if_updated(w); },
                    [&] { return base.send_max_bidi_streams(w); });
            }

            IOResult send(frame::fwriter& w, auto&& observer) {
                auto res = send_max_data(w, observer);
                if (res == IOResult::fatal) {
                    return res;
                }
                res = send_max_uni_streams(w, observer);
                if (res == IOResult::fatal) {
                    return res;
                }
                return send_max_bidi_streams(w, observer);
            }

            bool recv_max_data(const frame::MaxDataFrame& frame) {
                return base.recv_max_data(frame);
            }

            error::Error recv_max_streams(const frame::MaxStreamsFrame& frame) {
                return base.recv_max_streams(frame);
            }

            error::Error recv(const frame::Frame& frame) {
                return base.recv(frame);
            }
        };
    }  // namespace fnet::quic::stream::impl
}  // namespace utils
