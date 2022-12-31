/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// stream - QUIC stream
#pragma once
#include <cstdint>
#include "../transport_error.h"
#include "../frame/frame.h"
#include <concepts>
#include "../../bytelen_split.h"

namespace utils {
    namespace dnet {
        namespace quic::stream {

            enum class Direction {
                client,
                server,
                unknown,
            };

            enum class StreamType {
                uni,
                bidi,
                unknown,
            };

            constexpr size_t dir_to_mask(Direction dir) {
                switch (dir) {
                    case Direction::client:
                        return 0x0;
                    case Direction::server:
                        return 0x1;
                    default:
                        return ~0;
                }
            }

            constexpr Direction inverse(Direction dir) {
                switch (dir) {
                    case Direction::client:
                        return Direction::server;
                    case Direction::server:
                        return Direction::client;
                    default:
                        return Direction::unknown;
                }
            }

            constexpr size_t type_to_mask(StreamType typ) {
                switch (typ) {
                    case StreamType::uni:
                        return 0x0;
                    case StreamType::bidi:
                        return 0x2;
                    default:
                        return ~0;
                }
            }

            struct StreamID {
                size_t id = ~0;

                constexpr StreamID(size_t id)
                    : id(id) {}

                constexpr bool valid() const {
                    return id < (size_t(0x3) << 62);
                }

                constexpr operator size_t() const {
                    return id;
                }

                constexpr size_t seq_count() const {
                    if (!valid()) {
                        return ~0;
                    }
                    return id >> 2;
                }

                constexpr StreamType type() const {
                    if (!valid()) {
                        return StreamType::unknown;
                    }
                    if (id & 0x2) {
                        return StreamType::bidi;
                    }
                    else {
                        return StreamType::uni;
                    }
                }

                constexpr Direction dir() const {
                    if (!valid()) {
                        return Direction::unknown;
                    }
                    if (id & 0x1) {
                        return Direction::server;
                    }
                    else {
                        return Direction::client;
                    }
                }
            };

            constexpr StreamID invalid_id = ~0;

            template <class T>
            struct ACKWaitValue {
               private:
                bool wait_ack = false;
                size_t packet_number = ~0;
                T value{};

               public:
                void wait(T v) {
                    value = v;
                    wait_ack = true;
                    packet_number = ~0;
                }

                bool set_packet_number(size_t pn) {
                    if (packet_number != ~0) {
                        return false;
                    }
                    packet_number = pn;
                    return true;
                }

                bool get_packet_number(size_t& pn) {
                    if (packet_number == ~0) {
                        return false;
                    }
                    pn = packet_number;
                    return true;
                }

                bool waiting() const {
                    return wait_ack;
                }

                bool peek(T& v) const {
                    if (!wait_ack) {
                        return false;
                    }
                    v = value;
                    return true;
                }

                bool ack(size_t pn, T& v) {
                    if (!wait_ack ||
                        pn != packet_number) {
                        return false;
                    }
                    v = value;
                    wait_ack = false;
                    packet_number = ~0;
                    return true;
                }
            };

            struct Limiter {
               private:
                size_t limit = 0;
                size_t used = 0;

               public:
                constexpr Limiter() = default;
                constexpr size_t avail_size() const {
                    return limit - used;
                }

                constexpr bool on_limit(size_t s) const {
                    return avail_size() < s;
                }

                constexpr void update_limit(size_t new_limit) {
                    if (new_limit > limit) {
                        limit = new_limit;
                    }
                }

                constexpr bool use(size_t s) {
                    if (on_limit(s)) {
                        return false;
                    }
                    used += s;
                    return true;
                }

                constexpr bool update_use(size_t new_used) {
                    if (limit < new_used) {
                        return false;
                    }
                    if (new_used > used) {
                        used = new_used;
                    }
                    return true;
                }

                constexpr size_t curlimit() const {
                    return limit;
                }

                constexpr size_t curused() const {
                    return used;
                }

                bool unuse(size_t l) {
                    if (used < l) {
                        return false;
                    }
                    used -= l;
                    return true;
                }
            };

            struct StreamIDIssuer {
                size_t seq_count = 0;
                Limiter limit;
                const StreamType type{};
                Direction dir = Direction::unknown;

                constexpr StreamIDIssuer(StreamType type)
                    : type(type) {}

                constexpr void set_dir(Direction dir) {
                    this->dir = dir;
                }

                constexpr StreamID next() const {
                    return (seq_count << 2) | type_to_mask(type) | dir_to_mask(dir);
                }

                constexpr StreamID issue() {
                    auto res = next();
                    if (!res.valid()) {
                        return invalid_id;
                    }
                    if (!limit.use(1)) {
                        return invalid_id;
                    }
                    seq_count++;
                    return res;
                }

                constexpr void cancel_issue() {
                    if (seq_count != 0) {
                        seq_count--;
                    }
                }

                error::Error update_limit(size_t new_limit) {
                    if (new_limit >= size_t(1) << 60) {
                        return QUICError{
                            .msg = "MAX_STREAMS limit is over 2^60",
                            .rfc_ref = "RFC9000 4.6 Controlling Concurrency",
                            .transport_error = TransportError::FRAME_ENCODING_ERROR,
                            .frame_type = type == StreamType::bidi ? FrameType::MAX_STREAMS_BIDI : FrameType::MAX_STREAMS_UNI,
                        };
                    }
                    limit.update_limit(new_limit);
                    return error::none;
                }
            };

            struct LimiterACK {
                Limiter limit;
                ACKWaitValue<size_t> next_limit;

                size_t set_next_limit(size_t new_limit) {
                    if (new_limit >= size_t(1) << 60 ||
                        new_limit < limit.curlimit()) {
                        return limit.curlimit();
                    }
                    if (size_t v; next_limit.peek(v) && (v > new_limit)) {
                        next_limit.wait(v);  // reset packet number
                        return v;
                    }
                    next_limit.wait(new_limit);
                    return new_limit;
                }

                bool update_limit(size_t pn) {
                    if (size_t v; next_limit.ack(pn, v)) {
                        limit.update_limit(v);
                        return true;
                    }
                    return false;
                }
            };

            struct StreamIDAcceptor {
                size_t max_closed = 0;
                Limiter limit;
                const StreamType type{};
                Direction dir = Direction::unknown;

                constexpr StreamIDAcceptor(StreamType type)
                    : type(type) {}

                constexpr void set_dir(Direction dir) {
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
                    if (!limit.update_use(new_id.seq_count())) {
                        return QUICError{
                            .msg = "stream over concurrency limit received",
                            .transport_error = TransportError::FLOW_CONTROL_ERROR,
                        };
                    }
                    return error::none;
                }

                constexpr size_t update_limit(size_t new_limit) {
                    limit.update_limit(new_limit);
                    return limit.curlimit();
                }
            };

            enum class SendState {
                ready,
                send,
                data_sent,
                data_recved,
                reset_sent,
                reset_recved,
            };

            enum class RecvState {
                pre_recv,
                recv,
                size_known,
                data_recved,
                reset_recved,
                data_read,
                reset_read,
            };

            namespace internal {
                auto neq(auto cur) {
                    return [=](auto& state) {
                        return state != cur;
                    };
                }

                auto eq(auto cur) {
                    return [=](auto& state) {
                        return state == cur;
                    };
                }

                auto and_(auto... cond) {
                    return [=](auto& state) {
                        auto fn = [&](auto& cond) {
                            return cond(state);
                        };
                        return (... && fn(cond));
                    };
                }

                constexpr bool state_fn(auto& state, auto cond, auto next) {
                    if (!cond(state)) {
                        return false;
                    }
                    state = next;
                    return true;
                }
            }  // namespace internal

            struct SendUniStreamState {
                Limiter limit;
                bool stop_recv = false;
                SendState state = SendState::ready;
                size_t error_code = 0;
                ACKWaitValue<bool> wait_reset;

                size_t sendable_size() const {
                    return limit.avail_size();
                }

                bool block(size_t s) const {
                    return limit.on_limit(s);
                }

                void update_limit(size_t l) {
                    limit.update_limit(l);
                }

                bool can_send() const {
                    return state == SendState::ready ||
                           state == SendState::send;
                }

                bool reset(size_t code) {
                    if (state == SendState::reset_sent) {
                        return true;  // for retransmit
                    }
                    if (!internal::state_fn(
                            state,
                            internal::and_(
                                internal::neq(SendState::data_recved),
                                internal::neq(SendState::reset_recved)),
                            SendState::reset_sent)) {
                        return false;
                    }
                    error_code = code;
                    wait_reset.wait(true);
                    return true;
                }

                bool reset_ack(size_t pn) {
                    if (!internal::state_fn(
                            state,
                            internal::eq(SendState::reset_sent),
                            SendState::reset_recved)) {
                        return false;
                    }
                    if (bool v; !wait_reset.ack(pn, v)) {
                        state = SendState::reset_sent;
                        return false;
                    }
                    return true;
                }

                bool send(size_t s) {
                    auto cond = [&](auto&) {
                        return can_send() && limit.use(s);
                    };
                    return internal::state_fn(state, cond, SendState::send);
                }

                bool sent() {
                    return internal::state_fn(
                        state,
                        internal::eq(SendState::send),
                        SendState::data_sent);
                }

                bool sent_ack() {
                    return internal::state_fn(
                        state,
                        internal::eq(SendState::data_sent),
                        SendState::data_recved);
                }
            };

            struct RecvUniStreamState {
                Limiter limit;
                size_t final_size = 0;
                size_t error_code = 0;
                bool stop_sent = false;
                RecvState state = RecvState::pre_recv;

                size_t recvable_size() const {
                    return limit.avail_size();
                }

                bool block(size_t s) const {
                    return limit.on_limit(s);
                }

                size_t update_limit(size_t l) {
                    limit.update_limit(l);
                    return limit.curlimit();
                }

                bool can_recv() const {
                    return state == RecvState::pre_recv ||
                           state == RecvState::recv ||
                           state == RecvState::size_known;
                }

                // rfc 9000 4.4. Handling Stream Cancellation
                bool should_ignore() const {
                    return state == RecvState::reset_read ||
                           state == RecvState::reset_recved;
                }

                bool recv(size_t s) {
                    auto cond = [&](auto&) {
                        return can_recv() && limit.use(s);
                    };
                    return internal::state_fn(
                        state, cond,
                        state == RecvState::size_known ? RecvState::size_known : RecvState::recv);
                }

                bool size_known(size_t fin_size) {
                    if (!internal::state_fn(
                            state,
                            internal::eq(RecvState::recv),
                            RecvState::size_known)) {
                        return false;
                    }
                    final_size = fin_size;
                    return true;
                }

                bool size_known_state() {
                    return state == RecvState::reset_recved ||
                           state == RecvState::size_known;
                }

                bool reset(size_t fin_size, size_t code) {
                    auto was_known = size_known_state();
                    if (!internal::state_fn(
                            state,
                            internal::and_(
                                internal::neq(RecvState::data_read),
                                internal::neq(RecvState::reset_read)),
                            RecvState::reset_recved)) {
                        return false;
                    }
                    if (was_known) {
                        if (final_size != fin_size) {
                            return false;
                        }
                    }
                    final_size = fin_size;
                    error_code = code;
                    return true;
                }

                bool reset_read() {
                    return internal::state_fn(
                        state,
                        internal::eq(RecvState::reset_recved),
                        RecvState::reset_read);
                }

                bool recved() {
                    return internal::state_fn(
                        state,
                        internal::eq(RecvState::size_known),
                        RecvState::data_recved);
                }

                bool data_read() {
                    return internal::state_fn(
                        state,
                        internal::eq(RecvState::data_recved),
                        RecvState::data_read);
                }

                bool stop_sending() {
                    if (state == RecvState::reset_recved ||
                        state == RecvState::reset_read) {
                        return false;
                    }
                    stop_sent = true;
                    return true;
                }
            };

            struct SendUniStream {
                StreamID id;
                SendUniStreamState state;
            };

            struct RecvUniStream {
                StreamID id;
                RecvUniStreamState state;
            };

            struct BidiStream {
                StreamID id;
                SendUniStreamState send;
                RecvUniStreamState recv;
            };

            struct StreamFragment {
                size_t packet_number = ~0;
                bool ack = false;
                bool lost = false;

                size_t offset = 0;
                ByteLen data;
                bool fin = false;
            };

            struct ConnLimit {
                Limiter send;
                Limiter recv;
            };

            struct InitialLimits {
                size_t conn_data_limit = 0;
                size_t bidi_stream_limit = 0;
                size_t uni_stream_limit = 0;
                size_t stream_data_limit = 0;
            };

            struct StreamsState {
                StreamIDIssuer uni_issuer;
                StreamIDIssuer bidi_issuer;
                StreamIDAcceptor uni_acceptor;
                StreamIDAcceptor bidi_acceptor;
                ConnLimit conn;
                InitialLimits send_ini_limit;
                InitialLimits recv_ini_limit;

                constexpr StreamsState()
                    : uni_issuer(StreamType::uni),
                      bidi_issuer(StreamType::bidi),
                      uni_acceptor(StreamType::uni),
                      bidi_acceptor(StreamType::bidi) {}
                void set_dir(Direction dir) {
                    uni_issuer.set_dir(dir);
                    bidi_issuer.set_dir(dir);
                    uni_acceptor.set_dir(inverse(dir));
                    bidi_acceptor.set_dir(inverse(dir));
                }

                void set_send_initial_limit(InitialLimits lim) {
                    send_ini_limit = lim;
                    uni_issuer.update_limit(lim.uni_stream_limit);
                    bidi_issuer.update_limit(lim.bidi_stream_limit);
                    conn.send.update_limit(lim.conn_data_limit);
                }

                void set_recv_initial_limit(InitialLimits lim) {
                    recv_ini_limit = lim;
                    uni_acceptor.update_limit(lim.uni_stream_limit);
                    bidi_acceptor.update_limit(lim.bidi_stream_limit);
                    conn.recv.update_limit(lim.conn_data_limit);
                }

                void apply_limit(SendUniStreamState& s) {
                    s.update_limit(send_ini_limit.stream_data_limit);
                }

                void apply_limit(RecvUniStreamState& s) {
                    s.update_limit(recv_ini_limit.stream_data_limit);
                }
            };

            enum class OpenReason {
                recv_frame,
                send_frame,
                higher_open,
            };

            struct UserError {
                const char* msg = nullptr;
                error::Error base;
                void error(auto&& pb) {
                    helper::append(pb, msg);
                    if (base) {
                        helper::append(pb, " base=");
                        base.error(pb);
                    }
                }

                error::Error unwrap() const {
                    return base;
                }
            };

            struct StreamBuf {
                ByteLen buf;
                size_t offset = 0;
                bool fin = false;
            };

            template <class C, class H>
            concept StreamController =
                requires(C& c, H& h) {
                    { c.exists(h, invalid_id) } -> std::same_as<bool>;
                    { c.create_stream(h, invalid_id, OpenReason{}) } -> std::convertible_to<error::Error>;
                    { c.recv_state(h, invalid_id) } -> std::same_as<RecvUniStreamState*>;
                    { c.send_state(h, invalid_id) } -> std::same_as<SendUniStreamState*>;
                    { c.recv_stream(h, invalid_id, size_t{/*offset*/}, ByteLen{/*data*/}) } -> std::convertible_to<error::Error>;
                    { c.data_block(h, Limiter{}, size_t{/*peer limit*/}) } -> std::convertible_to<error::Error>;
                    { c.max_data(h, Limiter{}, size_t{/*new max*/}) } -> std::convertible_to<error::Error>;
                    { c.streams_block(h, StreamType{}, Limiter{}, size_t{/*peer limit*/}) } -> std::convertible_to<error::Error>;
                    { c.max_streams(h, StreamType{}, Limiter{}, size_t{/*new max*/}) } -> std::convertible_to<error::Error>;
                    { c.stream_data_block(h, invalid_id, Limiter{}, size_t{/*peer limit*/}) } -> std::convertible_to<error::Error>;
                    { c.max_stream_data(h, invalid_id, Limiter{}, size_t{/*peer limit*/}) } -> std::convertible_to<error::Error>;
                    { c.get_new_stream_data(h, invalid_id, std::declval<StreamBuf&>()) } -> std::same_as<bool>;
                    { c.stream_fragment(h, invalid_id, StreamFragment{}) } -> std::convertible_to<error::Error>;
                    { c.get_retransmit(h, invalid_id, std::declval<StreamFragment&>()) } -> std::same_as<bool>;
                    { c.submit_retransmit(h, invalid_id, StreamFragment{}) } -> std::convertible_to<error::Error>;
                    { c.get_split_len(h, invalid_id) } -> std::invocable<size_t, size_t, size_t>;
                    { c.on_send_packet(h, invalid_id, StreamFragment{}) } -> std::convertible_to<error::Error>;
                    { c.on_ack(h, size_t{/*packet number*/}) } -> std::convertible_to<error::Error>;
                    { c.on_lost(h, size_t{/*packet number*/}) } -> std::convertible_to<error::Error>;
                };

            constexpr auto err_known_offset = error::Error("known_offset", error::ErrorCategory::quicerr);

            struct DummyController {
                constexpr bool exists(auto&&, StreamID) const {
                    return false;
                }

                constexpr error::Error create_stream(auto&&, StreamID, OpenReason) const {
                    return error::unimplemented;
                }

                constexpr RecvUniStreamState* recv_state(auto&&, StreamID) const {
                    return nullptr;
                }

                constexpr SendUniStreamState* send_state(auto&&, StreamID) const {
                    return nullptr;
                }

                constexpr error::Error recv_stream(auto&&, StreamID, size_t, ByteLen) const {
                    return err_known_offset;
                }

                constexpr error::Error data_block(auto&&, Limiter, size_t) const {
                    return error::none;
                }

                constexpr error::Error max_data(auto&&, Limiter, size_t) const {
                    return error::none;
                }

                constexpr error::Error streams_block(auto&&, StreamType, Limiter, size_t) const {
                    return error::none;
                }

                constexpr error::Error max_streams(auto&&, StreamType, Limiter, size_t) const {
                    return error::none;
                }

                constexpr error::Error stream_data_block(auto&&, StreamID, Limiter, size_t) const {
                    return error::none;
                }

                constexpr error::Error max_stream_data(auto&&, StreamID, Limiter, size_t) const {
                    return error::none;
                }

                constexpr bool get_new_stream_data(auto&&, StreamID, StreamBuf&) const {
                    return false;
                }

                constexpr error::Error stream_fragment(auto&&, StreamID, StreamFragment) const {
                    return error::none;
                }

                constexpr bool get_retransmit(auto&&, StreamID, StreamFragment&) const {
                    return false;
                }

                constexpr error::Error submit_retransmit(auto&&, StreamID, StreamFragment) const {
                    return error::none;
                }

                constexpr auto get_split_len(auto&&, StreamID) const {
                    return spliter::const_len_fn(200);
                }

                constexpr error::Error on_send_packet(auto&&, StreamID, StreamFragment) const {
                    return error::none;
                }

                constexpr error::Error on_ack(auto&&, size_t packet_number) const {
                    return error::none;
                }

                constexpr error::Error on_lost(auto&&, size_t packet_number) const {
                    return error::none;
                }
            };

            static_assert(StreamController<DummyController, bool>, "DummyController not satisfies StreamController");

        }  // namespace quic::stream
    }      // namespace dnet
}  // namespace utils
