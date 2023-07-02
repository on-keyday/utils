/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "base.h"
#include "../initial_limits.h"
#include "../stream_id.h"

namespace utils {
    namespace fnet::quic::stream::core {
        struct StreamIDAcceptor {
            Limiter limit;
            const StreamType type{};
            Origin dir = Origin::unknown;
            bool should_send_limit_update = false;

            constexpr StreamIDAcceptor(StreamType type)
                : type(type) {}

            constexpr void set_dir(Origin dir) {
                this->dir = dir;
            }

            constexpr error::Error accept(StreamID new_id) {
                if (!new_id.valid() || new_id.type() != type) {
                    return QUICError{.msg = "invalid stream_id or type not matched. library bug!"};
                }
                if (new_id.dir() != dir) {
                    return QUICError{
                        .msg = "stream id provided by peer has not valid direction flag",
                        .transport_error = TransportError::STREAM_STATE_ERROR,
                    };
                }
                if (!limit.update_use(new_id.seq_count() + 1)) {
                    return QUICError{
                        .msg = "stream over concurrency limit received",
                        .transport_error = TransportError::FLOW_CONTROL_ERROR,
                    };
                }
                return error::none;
            }

            constexpr bool is_accepted(StreamID id) {
                return id.dir() == dir &&
                       id.type() == type &&
                       id.seq_count() < limit.curused();
            }

            constexpr bool update_limit(size_t new_limit) {
                auto res = limit.update_limit(new_limit);
                should_send_limit_update = res || should_send_limit_update;
                return res;
            }
        };

        struct StreamIDIssuer {
            Limiter limit;
            const StreamType type{};
            Origin dir = Origin::unknown;

            constexpr StreamIDIssuer(StreamType type)
                : type(type) {}

            constexpr void set_dir(Origin dir) {
                this->dir = dir;
            }

            constexpr StreamID next() const {
                return make_id(limit.curused(), dir, type);
            }

            constexpr StreamID issue() {
                auto res = next();
                if (!res.valid()) {
                    return invalid_id;
                }
                if (!limit.use(1)) {
                    return invalid_id;
                }
                return res;
            }

            constexpr bool is_issued(StreamID id) const {
                return id.dir() == dir &&
                       id.type() == type &&
                       id.seq_count() < limit.curused();
            }

            constexpr error::Error update_limit(size_t new_limit) {
                if (new_limit >= std::uint64_t(1) << 60) {
                    return QUICError{
                        .transport_error = TransportError::FRAME_ENCODING_ERROR,
                        .frame_type = type == StreamType::bidi ? FrameType::MAX_STREAMS_BIDI : FrameType::MAX_STREAMS_UNI,
                    };
                }
                limit.update_limit(new_limit);
                return error::none;
            }
        };

        struct ConnFlowLimit {
            Limiter send;
            Limiter recv;
            bool should_send_limit_update = false;

            bool update_recv_limit(std::uint64_t lim) {
                auto res = recv.update_limit(lim);
                should_send_limit_update = res || should_send_limit_update;
                return res;
            }
        };

        struct StreamsState {
            StreamIDIssuer uni_issuer;
            StreamIDIssuer bidi_issuer;
            StreamIDAcceptor uni_acceptor;
            StreamIDAcceptor bidi_acceptor;
            ConnFlowLimit conn_flow;
            InitialLimits send_ini_limit;
            InitialLimits recv_ini_limit;

            constexpr StreamsState()
                : uni_issuer(StreamType::uni),
                  bidi_issuer(StreamType::bidi),
                  uni_acceptor(StreamType::uni),
                  bidi_acceptor(StreamType::bidi) {}
            void set_dir(Origin dir) {
                uni_issuer.set_dir(dir);
                bidi_issuer.set_dir(dir);
                uni_acceptor.set_dir(inverse(dir));
                bidi_acceptor.set_dir(inverse(dir));
            }

            Origin peer_dir() const {
                return uni_acceptor.dir;
            }

            Origin local_dir() const {
                return uni_issuer.dir;
            }

            void set_send_initial_limit(InitialLimits lim) {
                send_ini_limit = lim;
                uni_issuer.limit.set_limit(lim.uni_stream_limit);
                bidi_issuer.limit.set_limit(lim.bidi_stream_limit);
                conn_flow.send.set_limit(lim.conn_data_limit);
            }

            void set_recv_initial_limit(InitialLimits lim) {
                recv_ini_limit = lim;
                uni_acceptor.limit.set_limit(lim.uni_stream_limit);
                bidi_acceptor.limit.set_limit(lim.bidi_stream_limit);
                conn_flow.recv.set_limit(lim.conn_data_limit);
            }
        };
    }  // namespace fnet::quic::stream::core
}  // namespace utils
