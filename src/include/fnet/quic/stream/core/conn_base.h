/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../../error.h"
#include "../../transport_error.h"
#include "issue_accept.h"
#include <helper/defer.h>
#include "../../frame/conn_manage.h"
#include "../fragment.h"
#include "../../ioresult.h"
#include "../../frame/writer.h"

namespace utils {
    namespace fnet::quic::stream {

        template <class TypeConfigs>
        struct ConnectionBase {
            using Lock = typename TypeConfigs::conn_lock;
            StreamsState state;
            Lock send_locker;
            Lock recv_locker;
            Lock accept_bidi_locker;
            Lock accept_uni_locker;
            Lock open_bidi_locker;
            Lock open_uni_locker;
            bool initial_set = false;

            // don't lock by yourself
            void set_initial_limits(const InitialLimits& local, const InitialLimits& peer) {
                {
                    const auto lock = do_multi_lock(recv_locker, accept_bidi_locker, accept_uni_locker);
                    state.set_recv_initial_limit(local);
                }
                {
                    const auto lock = do_multi_lock(send_locker, open_bidi_locker, open_uni_locker);
                    state.set_send_initial_limit(peer);
                }
            }

            // don't lock by yourself
            void set_local_initial_limits(const InitialLimits& local) {
                const auto lock = do_multi_lock(recv_locker, accept_bidi_locker, accept_uni_locker);
                state.set_recv_initial_limit(local);
            }

            // don't lock by yourself
            void set_peer_initial_limits(const InitialLimits& peer) {
                const auto lock = do_multi_lock(send_locker, open_bidi_locker, open_uni_locker);
                state.set_send_initial_limit(peer);
            }

            auto recv_lock() {
                return do_lock(recv_locker);
            }

            auto accept_bidi_lock() {
                return do_lock(accept_bidi_locker);
            }

            auto accept_uni_lock() {
                return do_lock(accept_uni_locker);
            }

            auto open_bidi_lock() {
                return do_lock(open_bidi_locker);
            }

            auto open_uni_lock() {
                return do_lock(open_uni_locker);
            }

            // called by each stream
            // you shouldn't get lock by yourself
            // cb is std::uint64_t(std::uint64_t avail_size,std::uint64_t limit)
            constexpr bool use_send_credit(auto&& cb) {
                const auto locked = do_lock(send_locker);
                std::uint64_t reqsize = cb(state.conn.send.avail_size(), state.conn.send.curlimit());
                if (reqsize == 0) {
                    return true;
                }
                return state.conn.send.use(reqsize);
            }

            // called by each stream
            // you shouldn't get lock by yourself
            constexpr bool use_recv_credit(std::uint64_t use_delta) {
                const auto locked = do_lock(recv_locker);
                return state.conn.recv.use(use_delta);
            }

            // you shouldn't get lock by yourself
            constexpr bool recv_max_data(const frame::MaxDataFrame& limit) {
                const auto locked = do_lock(send_locker);
                return state.conn.send.update_limit(limit.max_data);
            }

            // you shouldn't get lock by yourself
            constexpr error::Error recv_max_streams(const frame::MaxStreamsFrame& limit) {
                if (limit.type.type_detail() == FrameType::MAX_STREAMS_UNI) {
                    const auto locked = do_lock(open_uni_locker);
                    return state.uni_issuer.update_limit(limit.max_streams);
                }
                else if (limit.type.type_detail() == FrameType::MAX_STREAMS_BIDI) {
                    const auto locked = do_lock(open_bidi_locker);
                    return state.bidi_issuer.update_limit(limit.max_streams);
                }
                return error::none;
            }

            constexpr error::Error recv(const frame::Frame& frame) {
                if (is_MAX_STREAMS(frame.type.type_detail())) {
                    return recv_max_streams(static_cast<const frame::MaxStreamsFrame&>(frame));
                }
                if (frame.type.type_detail() == FrameType::MAX_DATA) {
                    recv_max_data(static_cast<const frame::MaxDataFrame&>(frame));
                    return error::none;
                }
                if (frame.type.type_detail() == FrameType::DATA_BLOCKED ||
                    is_STREAMS_BLOCKED(frame.type.type_detail())) {
                    return error::none;
                }
                return QUICError{.msg = "unexpected frame type. expect MAX_STREAMS(bidi/uni) or MAX_DATA"};
            }

            constexpr bool update_max_data(auto&& decide_new_limit) {
                auto new_limit = decide_new_limit(std::as_const(state.conn.recv));
                return state.conn.update_recv_limit(new_limit);
            }

            constexpr IOResult send_max_data(frame::fwriter& w) {
                frame::MaxDataFrame frame;
                frame.max_data = state.conn.recv.curlimit();
                if (w.remain().size() < frame.len()) {
                    return IOResult::no_capacity;
                }
                if (!w.write(frame)) {
                    return IOResult::fatal;
                }
                state.conn.should_send_limit_update = false;
                return IOResult::ok;
            }

            constexpr IOResult send_max_data_if_updated(frame::fwriter& w) {
                if (!state.conn.should_send_limit_update) {
                    return IOResult::not_in_io_state;
                }
                return send_max_data(w);
            }

            constexpr bool update_max_uni_streams(auto&& decide_new_limit) {
                auto new_limit = decide_new_limit(std::as_const(state.uni_acceptor.limit));
                return state.uni_acceptor.update_limit(new_limit);
            }

            constexpr bool update_max_bidi_streams(auto&& decide_new_limit) {
                auto new_limit = decide_new_limit(std::as_const(state.bidi_acceptor.limit));
                return state.bidi_acceptor.update_limit(new_limit);
            }

            constexpr IOResult send_max_uni_streams(frame::fwriter& w) {
                frame::MaxStreamsFrame frame;
                frame.type = FrameType::MAX_STREAMS_UNI;
                frame.max_streams = state.uni_acceptor.limit.curlimit();
                if (w.remain().size() < frame.len()) {
                    return IOResult::no_capacity;
                }
                if (!w.write(frame)) {
                    return IOResult::fatal;
                }
                state.uni_acceptor.should_send_limit_update = false;
                return IOResult::ok;
            }

            constexpr IOResult send_max_uni_streams_if_updated(frame::fwriter& w) {
                if (!state.uni_acceptor.should_send_limit_update) {
                    return IOResult::not_in_io_state;
                }
                return send_max_uni_streams(w);
            }

            constexpr IOResult send_max_bidi_streams(frame::fwriter& w) {
                frame::MaxStreamsFrame frame;
                frame.type = FrameType::MAX_STREAMS_BIDI;
                frame.max_streams = state.bidi_acceptor.limit.curlimit();
                if (w.remain().size() < frame.len()) {
                    return IOResult::no_capacity;
                }
                if (!w.write(frame)) {
                    return IOResult::fatal;
                }
                state.bidi_acceptor.should_send_limit_update = false;
                return IOResult::ok;
            }

            constexpr IOResult send_max_bidi_streams_if_updated(frame::fwriter& w) {
                if (!state.bidi_acceptor.should_send_limit_update) {
                    return IOResult::not_in_io_state;
                }
                return send_max_bidi_streams(w);
            }

           private:
            constexpr void open_impl(auto& locker, auto& issuer, auto&& cb) {
                const auto locked = do_lock(locker);
                auto id = issuer.issue();
                if (id == invalid_id) {
                    cb(id, issuer.limit.on_limit(1));
                    return;
                }
                cb(id, false);
            }

            constexpr void accept_impl(StreamID id, auto& locker, auto& acceptor, auto&& cb) {
                const auto locked = do_lock(locker);
                size_t curused = acceptor.limit.curused();
                auto err = acceptor.accept(id);
                if (err) {
                    cb(id, false, err);
                    return;
                }
                for (auto i = curused; i < acceptor.limit.curused(); i++) {
                    cb(make_id(i, acceptor.dir, acceptor.type), i != id.seq_count(), error::none);
                }
            }

           public:
            // cb(id,is_blocked)
            // if id==invalid_id and is_blocked is true, creation blocked by peer,
            // if id==invalid_id and is_blocked is false, creation limit reached
            // you shouldn't get lock by yourself
            constexpr void open_uni(auto&& cb) {
                return open_impl(open_uni_locker, state.uni_issuer, cb);
            }

            // cb(id,is_blocked)
            // if id==invalid_id and is_blocked is true, creation blocked by peer,
            // if id==invalid_id and is_blocked is false, creation limit reached
            // you shouldn't get lock by yourself
            constexpr void open_bidi(auto&& cb) {
                return open_impl(open_bidi_locker, state.bidi_issuer, cb);
            }

            // cb(id,by_higher_creation,err)
            // if by_higher_creation is true, this is created by higher stream id stream creation
            // you shouldn't get lock by yourself
            constexpr void accept(StreamID id, auto&& cb) {
                if (id.type() == StreamType::unknown) {
                    cb(id, false, QUICError{
                                      .msg = "invalid stream id by peer? or library bug",
                                      .transport_error = TransportError::PROTOCOL_VIOLATION,
                                  });
                }
                if (id.type() == StreamType::bidi) {
                    return accept_impl(id, accept_bidi_locker, state.bidi_acceptor, cb);
                }
                else {  // uni
                    return accept_impl(id, accept_uni_locker, state.uni_acceptor, cb);
                }
            }

            constexpr std::pair<bool, error::Error> check_recv_frame(FrameType type, StreamID id) {
                if (id.dir() == state.local_dir()) {
                    if (id.type() == StreamType::bidi) {
                        const auto locked = do_lock(open_bidi_locker);
                        // check whether id is already issued id
                        if (!state.bidi_issuer.is_issued(id)) {
                            return {true,
                                    QUICError{
                                        .msg = "not created stream id on bidirectional stream",
                                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                                        .frame_type = type,
                                    }};
                        }
                        // check FrameType (this detect library user's bug)
                        if (!is_BidiStreamOK(type)) {
                            return {
                                true,
                                QUICError{
                                    .msg = "not allowed frame type for bidirectional stream. library bug!",
                                    .transport_error = TransportError::STREAM_STATE_ERROR,
                                    .frame_type = type,
                                }};
                        }
                        return {true, error::none};
                    }
                    else if (id.type() == StreamType::uni) {
                        const auto locked = do_lock(open_uni_locker);
                        // check whether id is already issued id
                        if (!state.uni_issuer.is_issued(id)) {
                            return {true,
                                    QUICError{
                                        .msg = "not created stream id on unidirectional stream",
                                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                                        .frame_type = type,
                                    }};
                        }
                        // check FrameType (this detect peer's error)
                        if (!is_UniStreamReceiverOK(type)) {
                            return {
                                true,
                                QUICError{
                                    .msg = "not allowed frame type for sending unidirectional stream.",
                                    .transport_error = TransportError::STREAM_STATE_ERROR,
                                    .frame_type = type,
                                }};
                        }
                        return {true, error::none};
                    }
                }
                else if (id.dir() == state.peer_dir()) {
                    if (id.type() == StreamType::bidi) {
                        // check FrameType (this detect library user's bug)
                        if (!is_BidiStreamOK(type)) {
                            return {
                                true,
                                QUICError{
                                    .msg = "not allowed frame type for bidirectional stream. library bug!",
                                    .transport_error = TransportError::STREAM_STATE_ERROR,
                                    .frame_type = type,
                                }};
                        }
                        return {false, error::none};
                    }
                    else {
                        // check FrameType (this detect peer's error)
                        if (!is_UniStreamSenderOK(type)) {
                            return {
                                true,
                                QUICError{
                                    .msg = "not allowed frame type for receiving unidirectional stream.",
                                    .transport_error = TransportError::STREAM_STATE_ERROR,
                                    .frame_type = type,
                                }};
                        }
                        return {false, error::none};
                    }
                }
                return {
                    false,
                    QUICError{.msg = "unexpected stream type. library bug!!"},
                };
            }
        };

        constexpr IOResult send_conn_blocked(frame::fwriter& w, std::uint64_t max_data) noexcept {
            if (max_data >= varint::border(8)) {
                return IOResult::invalid_data;
            }
            frame::DataBlockedFrame blocked;
            blocked.max_data = max_data;
            if (w.remain().size() < blocked.len()) {
                return IOResult::no_capacity;
            }
            return w.write(blocked) ? IOResult::ok : IOResult::fatal;
        }

        constexpr IOResult send_streams_blocked(frame::fwriter& w, std::uint64_t max_streams, bool uni) noexcept {
            if (max_streams >= varint::border(8)) {
                return IOResult::invalid_data;
            }
            frame::StreamsBlockedFrame blocked;
            blocked.type = uni ? FrameType::MAX_STREAMS_UNI : FrameType::MAX_STREAMS_BIDI;
            blocked.max_streams = max_streams;
            if (w.remain().size() < blocked.len()) {
                return IOResult::no_capacity;
            }
            return w.write(blocked) ? IOResult::ok : IOResult::fatal;
        }

    }  // namespace fnet::quic::stream
}  // namespace utils
