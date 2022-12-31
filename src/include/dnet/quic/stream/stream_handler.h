/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "stream.h"
#include "../../bytelen_split.h"
#include "../frame_old/frame_make.h"
#include "../frame_old/frame_util.h"

namespace utils {
    namespace dnet {
        namespace quic::stream {

            template <class Holder, class Controller>
            struct StreamCommonHandler {
                StreamsState state;
                Holder h;
                Controller ctrl;

                bool exists_id(StreamID id) {
                    return ctrl.exists(h, id);
                }

                error::Error access_recv_state(StreamID id, auto&& cb) {
                    RecvUniStreamState* state = ctrl.recv_state(h, id);
                    if (!state) {
                        return QUICError{
                            .msg = "cannot access recv state machine",
                        };
                    }
                    this->state.apply_limit(*state);
                    return cb(*state);
                }

                error::Error access_send_state(StreamID id, auto&& cb) {
                    SendUniStreamState* state = ctrl.send_state(h, id);
                    if (!state) {
                        return QUICError{
                            .msg = "cannot access send state machine",
                        };
                    }
                    this->state.apply_limit(*state);
                    return cb(*state);
                }

                Direction self_dir() const {
                    return state.uni_issuer.dir;
                }

                error::Error check_id_and_frame_type(StreamID id, FrameType type, bool send) {
                    if (!id.valid()) {
                        return QUICError{
                            .msg = "not valid frame type",
                        };
                    }
                    // check stream id and frame type
                    if (id.type() == StreamType::bidi) {
                        if (!is_BidiStreamOK(type)) {
                            return QUICError{
                                .msg = "frame type is not allowed for bidirectional stream",
                                .transport_error = TransportError::PROTOCOL_VIOLATION,
                                .frame_type = type,
                            };
                        }
                    }
                    else {
                        Direction sender = (send ? self_dir() : inverse(self_dir()));
                        if (id.dir() == sender) {
                            if (!is_UniStreamSenderOK(type)) {
                                return QUICError{
                                    .msg = "frame type is not allowed for unidirectional sending stream",
                                    .transport_error = TransportError::PROTOCOL_VIOLATION,
                                    .frame_type = type,
                                };
                            }
                        }
                        else {
                            if (!is_UniStreamReceiverOK(type)) {
                                return QUICError{
                                    .msg = "frame type is not allowed for unidirectional receipt stream",
                                    .transport_error = TransportError::PROTOCOL_VIOLATION,
                                    .frame_type = type,
                                };
                            }
                        }
                    }
                    return error::none;
                }
            };

            template <class Holder, class Controller>
            struct StreamRecvHandler {
               private:
                StreamCommonHandler<Holder, Controller>& c;

                template <class H, class C>
                friend struct Streams;

                constexpr StreamRecvHandler(StreamCommonHandler<Holder, Controller>& c)
                    : c(c) {}

                static error::Error accept(StreamIDAcceptor& acceptor, StreamID id, auto&& cb) {
                    size_t prev_limit = acceptor.limit.curused();
                    if (auto err = acceptor.accept(id)) {
                        return err;
                    }
                    for (size_t i = prev_limit + 1; i < id.id; i++) {
                        if (error::Error err = cb(i, OpenReason::higher_open)) {
                            return err;
                        }
                    }
                    if (error::Error err = cb(id, OpenReason::recv_frame)) {
                        return err;
                    }
                    return error::none;
                }

                error::Error accept_uni(StreamID id) {
                    return accept(c.state.uni_acceptor, id, [&](StreamID id, OpenReason reason) {
                        return c.ctrl.create_stream(c.h, id, reason);
                    });
                }

                error::Error accept_bidi(StreamID id) {
                    return accept(state.bidi_acceptor, id, [&](StreamID id, OpenReason reason) {
                        return c.ctrl.create_stream(c.h, id, reason);
                    });
                }

                error::Error update_recv_credit(RecvUniStreamState& s, FrameType type, size_t length) {
                    if (!s.recv(length)) {
                        return QUICError{
                            .msg = "stream flow control limit reached",
                            .transport_error = TransportError::FLOW_CONTROL_ERROR,
                            .frame_type = type,
                        };
                    }
                    if (!c.state.conn.recv.use(length)) {
                        return QUICError{
                            .msg = "connection flow control limit reached",
                            .transport_error = TransportError::FLOW_CONTROL_ERROR,
                            .frame_type = type,
                        };
                    }
                    return error::none;
                }

                error::Error check_id(StreamID id, FrameType type) {
                    if (auto err = c.check_id_and_frame_type(id, type, false)) {
                        return err;
                    }
                    if (!c.exists_id(id)) {
                        if (id.dir() == c.self_dir()) {
                            return QUICError{
                                .msg = "not yet created stream id by this endpoint",
                                .transport_error = TransportError::PROTOCOL_VIOLATION,
                                .frame_type = type,
                            };
                        }
                        else {
                            if (id.type() == StreamType::uni) {
                                return accept_uni(id);
                            }
                            else {
                                return accept_bidi(id);
                            }
                        }
                    }
                    return error::none;
                }

               public:
                error::Error handle_stream(const frame::StreamFrame& f) {
                    auto id = StreamID{f.streamID};
                    if (auto err = check_id(id, f.type.type_detail())) {
                        return err;
                    }
                    return c.access_recv_state(id, [&](RecvUniStreamState& s) -> error::Error {
                        if (s.should_ignore()) {
                            return error::none;  // ignore stream frame
                        }
                        if (error::Error err = c.ctrl.known_offset(c.h, id, f.offset, f.stream_data)) {
                            if (err == err_known_offset) {
                                return error::none;  // duplicated recv.
                            }
                            return err;
                        }
                        if (auto err = update_recv_credit(s, f.type.type_detail(), f.length)) {
                            return err;
                        }
                        if (f.type.STREAM_fin()) {
                            // TODO(on-keyday): should check error?
                            s.size_known(f.offset + f.length);
                        }
                        return error::none;
                    });
                }

                error::Error handle_reset_stream(const frame::ResetStreamFrame& reset) {
                    auto id = StreamID{reset.streamID};
                    if (auto err = check_id(id, reset.type.type_detail())) {
                        return err;
                    }
                    return c.access_recv_state(id, [&](RecvUniStreamState& s) -> error::Error {
                        auto known = s.size_known_state();
                        if (!s.reset(reset.final_size, reset.error_code)) {
                            if (known) {
                                return QUICError{
                                    .msg = "known size is not match to recved final_size",
                                    .transport_error = TransportError::FINAL_SIZE_ERROR,
                                };
                            }
                            // duplicated recv
                        }
                        if (!known) {
                            if (s.limit.curused() < reset.final_size) {
                                if (auto err = update_recv_credit(s, reset.type.type_detail(), reset.final_size - s.limit.curused())) {
                                    return err;
                                }
                            }
                        }
                        return error::none;
                    });
                }

                error::Error handle_data_blocked(const frame::DataBlockedFrame& block) {
                    size_t peer_known_limit = block.max_data;
                    return c.ctrl.data_block(c.h, std::as_const(state.conn.recv), peer_known_limit);
                }

                error::Error handle_max_data(const frame::MaxDataFrame& max_data) {
                    c.state.conn.send.update_limit(max_data.max_data);
                    return c.ctrl.max_data(c.h, std::as_const(c.state.conn.send), max_data.max_data);
                }

                error::Error handle_streams_blocked(const frame::StreamsBlockedFrame& block) {
                    StreamType type;
                    Limiter local_limit;
                    if (block.type.type_detail() == FrameType::STREAMS_BLOCKED_BIDI) {
                        type = StreamType::bidi;
                        local_limit = c.state.bidi_acceptor.limit;
                    }
                    else {
                        type = StreamType::uni;
                        local_limit = c.state.uni_acceptor.limit;
                    }
                    size_t peer_known_limit = block.max_streams;
                    return c.ctrl.streams_block(c.h, type, std::as_const(local_limit), peer_known_limit);
                }

                error::Error handle_max_streams(const frame::MaxStreamsFrame& max_streams) {
                    Limiter limit;
                    StreamType type;
                    if (max_streams.type.type_detail() == FrameType::MAX_STREAMS_BIDI) {
                        if (auto err = c.state.bidi_issuer.update_limit(max_streams.max_streams)) {
                            return err;
                        }
                        limit = c.state.bidi_issuer.limit;
                        type = StreamType::bidi;
                    }
                    else {
                        if (auto err = c.state.uni_issuer.update_limit(max_streams.max_streams)) {
                            return err;
                        }
                        limit = c.state.uni_issuer.limit;
                        type = StreamType::uni;
                    }
                    return c.ctrl.max_streams(c.h, type, std::as_const(limit), max_streams.max_streams)
                }

                error::Error handle_stream_data_blocked(const frame::StreamDataBlockedFrame& block) {
                    auto id = StreamID{block.streamID};
                    if (auto err = check_id(id, block.type.type_detail())) {
                        return err;
                    }
                    return c.access_recv_state(id, [&](RecvUniStreamState& s) -> error::Error {
                        size_t local_limit = s.limit.curlimit();
                        size_t peer_known_limit = block.max_stream_data;
                        return c.ctrl.stream_data_block(c.h, id, local_limit, peer_known_limit);
                    });
                }

                error::Error handle_max_stream_data(const frame::MaxStreamDataFrame& max_data) {
                    auto id = StreamID{max_data.streamID};
                    if (auto err = check_id(id, max_data.type.type_detail())) {
                        return err;
                    }
                    return c.access_send_state(id, [&](SendUniStreamState& s) -> error::Error {
                        s.update_limit(max_data.max_stream_data);
                        return c.ctrl.max_stream_data(c.h, id, std::as_const(s.limit), max_data.max_stream_data);
                    });
                }

                error::Error handle_stop_sending(const frame::StopSendingFrame& stop_sending) {
                    auto id = StreamID{stop_sending.streamID};
                    if (auto err = check_id(id, stop_sending.type.type_detail())) {
                        return err;
                    }
                    return c.access_send_state(id, [&](SendUniStreamState& s) -> error::Error {
                        s.stop_recv = true;
                        s.error_code = stop_sending.application_protocol_error_code;
                        return c.ctrl.stop_sending(c.h, id);
                    });
                }

                void set_intial_limit(InitialLimits limit) {
                    c.state.recv_ini_limit(limit);
                }
            };

            template <class Holder, class Controller>
            struct StreamSendHandler {
               private:
                StreamCommonHandler<Holder, Controller>& c;

                template <class H, class C>
                friend struct Streams;

                constexpr StreamSendHandler(StreamCommonHandler<Holder, Controller>& c)
                    : c(c) {}

                static std::pair<StreamID, error::Error> open(StreamIDIssuer& issuer, auto&& cb) {
                    auto id = issuer.issue();
                    if (!id.valid()) {
                        if (!issuer.next().valid()) {
                            return {
                                invalid_id,
                                QUICError{
                                    .msg = "quic streams limit reachs 2^60",
                                    .transport_error = TransportError::STREAM_LIMIT_ERROR,
                                }};
                        }
                        return {invalid_id, error::block};
                    }
                    if (error::Error err = cb(id)) {
                        issuer.cancel_issue();
                        return {invalid_id, err};
                    }
                    return {id, error::none};
                }

                error::Error check_id(StreamID id, FrameType type) {
                    if (!id.valid()) {
                        return UserError{"invalid id"};
                    }
                    if (!c.exists_id(id)) {
                        return UserError{"open stream before generate frame"};
                    }
                    if (auto err = c.check_id_and_frame_type(id, type, true)) {
                        return UserError{
                            .msg = "unexpected frame type for sender",
                            .base = std::move(err),
                        };
                    }
                    return error::none;
                }

                bool use_send_credit(SendUniStreamState& s, size_t use) {
                    if (use > s.limit.curused()) {
                        auto cur = s.limit.curlimit();
                        if (!s.send(use - cur)) {
                            return false;
                        }
                        if (!c.state.conn.send.use(use - cur)) {
                            s.limit.unuse(use - cur);
                            return false;
                        }
                    }
                    return true;
                }

                error::Error generate_first_time_stream(StreamID id, SendUniStreamState& s, auto&& cb) {
                    StreamBuf buf;
                    if (!c.ctrl.get_new_stream_data(c.h, id, buf)) {
                        return error::none;  // nothing to send
                    }
                    error::Error err;
                    auto gen_stream = [&](ByteLen b, size_t offset, bool fin) {
                        auto strm = frame::make_stream(nullptr, id, b, true, false, offset);
                        err = cb(strm);
                        if (err) {
                            return false;
                        }
                        StreamFragment f;
                        f.offset = offset;
                        f.data = b;
                        f.fin = fin;
                        f.packet_number = ~0;
                        err = c.ctrl.stream_fragment(c.h, id, f);
                        if (err) {
                            return false;
                        }
                        return true;
                    };
                    if (buf.buf.len == 0) {
                        gen_stream({}, buf.offset, buf.fin);
                        return err;
                    }
                    auto res = spliter::split(buf.buf, c.ctrl.get_split_len(c.h, id), [&](ByteLen b, size_t off, size_t rem) {
                        auto offset = buf.offset + off;
                        auto use = offset + b.len;
                        if (!use_send_credit(s, use)) {
                            if (s.limit.on_limit(use)) {
                                auto block = frame::make_stream_data_blocked(nullptr, id, s.limit.curlimit());
                                err = cb(block);
                                if (err) {
                                    return false;
                                }
                            }
                            if (c.state.conn.send.on_limit(use)) {
                                auto block = frame::make_data_blocked(nullptr, c.state.conn.send.curlimit());
                                err = cb(block);
                                if (err) {
                                    return false;
                                }
                            }
                            return false;
                        }
                        return gen_stream(b, offset, buf.fin && rem == 0);
                    });
                    if (err) {
                        return err;
                    }
                    return error::none;
                }

                error::Error generate_retransmit_stream(StreamID id, SendUniStreamState& s, auto&& cb) {
                    StreamFragment frag;
                    while (c.ctrl.get_retransmit(c.h, id, frag)) {
                        if (frag.offset + frag.data.len > s.limit.curused()) {
                            return QUICError{
                                .msg = "not sent bytes on retransmit scheduling",
                            };
                        }
                        auto strm = frame::make_stream(nullptr, id, frag.data, true, frag.fin, frag.offset);
                        if (auto err = cb(strm)) {
                            return err;
                        }
                        if (auto err = c.ctrl.submit_retransmit(c.h, id, frag)) {
                            return err;
                        }
                    }
                    return error::none;
                }

               public:
                std::pair<StreamID, error::Error> open_uni() {
                    return open(c.state.uni_issuer, [&](StreamID id) -> error::Error {
                        return c.ctrl.create_stream(c.h, id, OpenReason::send_frame);
                    });
                }

                std::pair<StreamID, error::Error> open_bidi() {
                    return open(c.state.uni_issuer, [&](StreamID id) -> error::Error {
                        return c.ctrl.create_stream(c.h, id, OpenReason::send_frame);
                    });
                }

                error::Error generate_streams_block(auto&& cb) {
                    if (c.state.bidi_issuer.limit.on_limit(1)) {
                        auto fr = frame::make_streams_blocked(nullptr, false, c.state.bidi_issuer.limit.curlimit());
                        if (error::Error err = cb(fr)) {
                            return err;
                        }
                    }
                    if (c.state.uni_issuer.limit.on_limit(1)) {
                        auto fr = frame::make_streams_blocked(nullptr, true, c.state.uni_issuer.limit.curlimit());
                        if (error::Error err = cb(fr)) {
                            return err;
                        }
                    }
                    return error::none;
                }

                // error::Error cb(Frame)
                error::Error generate_stream(StreamID id, auto&& cb) {
                    if (auto err = check_id(id, FrameType::STREAM)) {
                        return err;
                    }
                    return c.access_send_state(id, [&](SendUniStreamState& s) -> error::Error {
                        if (auto err = generate_first_time_stream(id, s, cb)) {
                            return err;
                        }
                        if (auto err = generate_retransmit_stream(id, s, cb)) {
                            return err;
                        }
                        return error::none;
                    });
                }

                error::Error generate_max_streams(size_t new_limit, StreamType type, auto&& cb) {
                    bool uni = type == StreamType::uni;
                    if (uni) {
                        new_limit = c.state.uni_acceptor.update_limit(new_limit);
                    }
                    else {
                        new_limit = c.state.bidi_acceptor.update_limit(new_limit);
                    }
                    auto fr = frame::make_max_streams(nullptr, uni, new_limit);
                    return cb(fr);
                }

                error::Error generate_max_data(size_t new_limit, auto&& cb) {
                    new_limit = c.state.conn.set_next_recv_limit(new_limit);
                    auto fr = frame::make_max_data(nullptr, new_limit);
                    return cb(fr);
                }

                error::Error generate_max_stream_data(StreamID id, size_t new_limit, auto&& cb) {
                    if (auto err = check_id(id, FrameType::MAX_STREAM_DATA)) {
                        return error::none;
                    }
                    return c.access_recv_state(id, [&](RecvUniStreamState& s) -> error::Error {
                        new_limit = s.update_limit(new_limit);
                        auto fr = frame::make_max_stream_data(nullptr, id, new_limit);
                        return cb(fr);
                    });
                }

                error::Error generate_reset_stream(StreamID id, size_t code, auto&& cb) {
                    if (auto err = check_id(id, FrameType::RESET_STREAM)) {
                        return err;
                    }
                    return c.access_send_state(id, [&](SendUniStreamState& s) -> error::Error {
                        if (!s.reset(code)) {
                            return UserError{"cannot reset stream at current state"};
                        }
                        auto fr = frame::make_reset_stream(nullptr, id, code, s.limit.curused());
                        return cb(fr);
                    });
                }

                error::Error generate_stop_sending(StreamID id, size_t code, auto&& cb) {
                    if (auto err = check_id(id, FrameType::STOP_SENDING)) {
                        return err;
                    }
                    return c.access_recv_state(id, [&](RecvUniStreamState& s) -> error::Error {
                        if (!s.stop_sending()) {
                            return UserError{"cannot stop sending at current state"};
                        }
                        auto fr = frame::make_stop_sending(nullptr, id, code);
                        return cb(fr);
                    });
                }

                error::Error on_send_packet(size_t packet_number, const frame::Frame* f) {
                    if (auto s = frame::frame_cast<frame::StreamFrame>(f)) {
                        StreamFragment frag;
                        auto id = StreamID{s->streamID};
                        frag.data = s->stream_data;
                        frag.offset = s->offset;
                        frag.packet_number = packet_number;
                        frag.fin = s->type.STREAM_fin();
                        return c.ctrl.on_send_packet(c.h, id, frag);
                    }
                    return error::none;
                }

                error::Error on_ack(size_t packet_number) {
                    return c.ctrl.on_ack(packet_number);
                }

                error::Error on_lost(size_t packet_number) {
                    return c.ctrl.on_lost(packet_number);
                }

                void set_intial_limit(InitialLimits limit) {
                    c.state.set_send_initial_limit(limit);
                }
            };

            template <class Holder, class Controller>
            struct Streams {
               private:
                static_assert(StreamController<Controller, Holder>, "Controller not satisfies StreamController");
                StreamCommonHandler<Holder, Controller> c;

               public:
                void set_direction(Direction dir) {
                    c.state.set_dir(dir);
                }

                StreamRecvHandler<Holder, Controller> recv() {
                    return {c};
                }

                StreamSendHandler<Holder, Controller> send() {
                    return {c};
                }

                Holder& data() {
                    return c.h;
                }
            };

        }  // namespace quic::stream
    }      // namespace dnet
}  // namespace utils
