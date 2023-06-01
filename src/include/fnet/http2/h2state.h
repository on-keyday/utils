/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// h2state - http2 state machines
#pragma once
#include "h2frame.h"
#include "stream_state.h"
#include "h2frame.h"
#include "h2err.h"
#include "h2settings.h"
#include "../std/deque.h"
#include "../dll/allocator.h"
#include "../storage.h"
#include "stream_id.h"
#include "../std/hash_map.h"
#include <wrap/light/enum.h>

namespace utils {
    namespace fnet::http2::stream {
        enum class State {
            idle,
            closed,
            open,
            half_closed_remote,
            half_closed_local,
            reserved_remote,
            reserved_local,
            unknown,
        };

        BEGIN_ENUM_STRING_MSG(State, status_name)
        ENUM_STRING_MSG(State::idle, "idle")
        ENUM_STRING_MSG(State::closed, "closed")
        ENUM_STRING_MSG(State::open, "open")
        ENUM_STRING_MSG(State::half_closed_local, "half_closed_local")
        ENUM_STRING_MSG(State::half_closed_remote, "half_closed_remote")
        ENUM_STRING_MSG(State::reserved_local, "reserved_local")
        ENUM_STRING_MSG(State::reserved_remote, "reserved_remote")
        END_ENUM_STRING_MSG("unknown")

        namespace policy {
            template <class T, T... allow>
            struct AllowPolicy {
                const char* debug = nullptr;
                constexpr bool operator()(T t) const noexcept {
                    auto f = [&](T v) {
                        return t == v;
                    };
                    return (... || f(allow));
                }
            };

            template <frame::Type... allow>
            using FrameAllowPolicy = AllowPolicy<frame::Type, allow...>;

            template <State... allow>
            using StateAllowPolicy = AllowPolicy<State, allow...>;

#define recv_debug_msg(frame, state) "receiving other frame than " frame " on " state " state"
#define send_debug_msg(frame, state) "sending other frame than " frame " on " state " state"

            constexpr auto recv_idle = FrameAllowPolicy<frame::Type::header, frame::Type::priority>{
                recv_debug_msg("HEADERS or PRIORITY", "idle"),
            };

            constexpr auto send_idle = FrameAllowPolicy<frame::Type::header, frame::Type::priority>{
                send_debug_msg("HEADERS or PRIORITY", "idle"),
            };

            constexpr auto recv_reserved_local = FrameAllowPolicy<frame::Type::priority, frame::Type::window_update, frame::Type::rst_stream>{
                recv_debug_msg("PRIORITY, WINDOW_UPDATE, or RST_STREAM", "reserved_local"),
            };

            constexpr auto send_reserved_local = FrameAllowPolicy<frame::Type::header, frame::Type::priority, frame::Type::rst_stream>{
                send_debug_msg("HEADER, PRIORITY, or RST_STREAM", "reserved_local"),
            };

            constexpr auto recv_reserved_remote = FrameAllowPolicy<frame::Type::header, frame::Type::priority, frame::Type::rst_stream>{
                recv_debug_msg("HEADER, PRIORITY, or RST_STREAM", "reserved_remote"),
            };

            constexpr auto send_reserved_remote = FrameAllowPolicy<frame::Type::priority, frame::Type::window_update, frame::Type::rst_stream>{
                send_debug_msg("PRIORITY, WINDOW_UPDATE, or RST_STREAM", "reserved_remote"),
            };

            constexpr auto has_end_stream_flag = FrameAllowPolicy<frame::Type::data, frame::Type::header>{};

            constexpr auto send_half_closed_local = FrameAllowPolicy<frame::Type::priority, frame::Type::window_update, frame::Type::rst_stream>{
                send_debug_msg("PRIORITY, WINDOW_UPDATE, or RST_STREAM", "half_closed_local"),
            };

            constexpr auto recv_half_closed_remote = FrameAllowPolicy<frame::Type::priority, frame::Type::window_update, frame::Type::rst_stream>{
                recv_debug_msg("PRIORITY, WINDOW_UPDATE, or RST_STREAM", "half_closed_remote"),
            };

            constexpr auto recv_closed = FrameAllowPolicy<frame::Type::priority>{
                recv_debug_msg("PRIORITY", "closed"),
            };

            constexpr auto send_closed = FrameAllowPolicy<frame::Type::priority>{
                send_debug_msg("PRIORITY", "closed"),
            };

#undef recv_debug_msg
#undef send_debug_msg

            constexpr auto recv_DATA = StateAllowPolicy<State::open, State::half_closed_local>{};
            constexpr auto send_DATA = StateAllowPolicy<State::open, State::half_closed_remote>{};
            constexpr auto recv_HEADERS = StateAllowPolicy<State::idle, State::open, State::reserved_remote, State::half_closed_local>{};
            constexpr auto send_HEADERS = StateAllowPolicy<State::idle, State::open, State::reserved_local, State::half_closed_remote>{};
            // PRIORITY is everywhere enabled
            constexpr auto recv_RST_STREAM = StateAllowPolicy<State::open, State::reserved_remote, State::reserved_local, State::half_closed_local, State::half_closed_remote>{};
            constexpr auto send_RST_STREAM = StateAllowPolicy<State::open, State::reserved_remote, State::reserved_local, State::half_closed_local, State::half_closed_remote>{};
            constexpr auto recv_PUSH_PROMISE = StateAllowPolicy<State::open, State::half_closed_local>{};
            constexpr auto send_PUSH_PROMISE = StateAllowPolicy<State::open, State::half_closed_remote>{};
            constexpr auto recv_WINDOW_UPDATE = StateAllowPolicy<State::open, State::reserved_local, State::half_closed_remote>{};
            constexpr auto send_WINDOW_UPDATE = StateAllowPolicy<State::open, State::reserved_remote, State::half_closed_local>{};

            // SETTINGS,PING,and GOAWAY are connection related frame
            // CONTINUATION is needed contexcical handling

        }  // namespace policy

        struct StreamNumCommonState {
            // stream id
            ID id = invalid_id;
            // stream state machine
            State state = State::idle;
            // error code
            std::uint32_t code = 0;
            // no need handling exclude PRIORITY
            bool perfect_closed = false;

            constexpr void maybe_close_by_higher_creation() {
                if (state == State::idle) {
                    state = State::closed;
                    perfect_closed = true;
                }
            }

            constexpr void on_promise_received() {
                state = State::reserved_remote;
            }

            constexpr void on_promise_sent() {
                state = State::reserved_local;
            }

            constexpr Error can_send(const frame::Frame& f) const {
                if (f.id != id) {
                    return Error{
                        H2Error::internal,
                        false,
                        "stream routing is incorrect. library bug!!",
                    };
                }
                auto can = [&](auto&& policy) {
                    if (!policy(state)) {
                        return Error{H2Error::protocol};
                    }
                    return no_error;
                };
                switch (f.type) {
                    default:
                        return Error{
                            H2Error::internal,
                            false,
                            "this frame is out of target",
                        };
                    case frame::Type::priority:
                        // RFC 9113 5.1. Stream States says:
                        // Note that PRIORITY can be sent and received in any stream state.
                        return no_error;
                    case frame::Type::data:
                        return can(policy::send_DATA);
                    case frame::Type::header:
                        return can(policy::send_HEADERS);
                    case frame::Type::rst_stream:
                        return can(policy::send_RST_STREAM);
                    case frame::Type::push_promise:
                        return can(policy::send_PUSH_PROMISE);
                    case frame::Type::window_update:
                        return can(policy::send_WINDOW_UPDATE);
                }
            }

            constexpr Error on_send(const frame::Frame& f) {
                if (id != f.id) {
                    return Error{
                        H2Error::internal,
                        false,
                        "stream routing is incorrect. library bug!!",
                    };
                }
                // RFC 9113 5.1. Stream States says:
                // Note that PRIORITY can be sent and received in any stream state.
                if (f.type == frame::Type::priority) {
                    return no_error;
                }
                if (state == State::idle) {
                    if (policy::send_idle(f.type)) {
                        return Error{
                            H2Error::protocol,
                            false,
                            policy::send_idle.debug,
                        };
                    }
                    if (f.type == frame::Type::header) {
                        if (id.by_server()) {
                            return Error{
                                H2Error::protocol,
                                false,
                                "sending HEADERS frame on idle with stream initiated by server",
                            };
                        }
                        state = State::open;
                    }
                }
                if (state == State::closed) {
                    if (!policy::send_closed(f.type)) {
                        return Error{
                            H2Error::protocol,
                            false,
                            policy::send_closed.debug,
                        };
                    }
                    return no_error;  // no more handling is needed
                }
                if (f.type == frame::Type::rst_stream) {
                    state = State::closed;
                    code = static_cast<const frame::RstStreamFrame&>(f).code;
                    // not perfectly closed
                    return no_error;
                }
                if (state == State::reserved_local) {
                    if (!policy::send_reserved_local(f.type)) {
                        return Error{
                            H2Error::protocol,
                            false,
                            policy::send_reserved_local.debug,
                        };
                    }
                    if (f.type == frame::Type::header) {
                        state = State::half_closed_remote;
                    }
                }
                if (state == State::reserved_remote) {
                    if (!policy::send_reserved_remote(f.type)) {
                        return Error{
                            H2Error::protocol,
                            false,
                            policy::send_reserved_remote.debug,
                        };
                    }
                }
                if (state == State::open) {
                    if (policy::has_end_stream_flag(f.type)) {
                        if (f.flag & frame::Flag::end_stream) {
                            state = State::half_closed_local;
                            return no_error;  // no more handling needed for this header or data frame
                        }
                    }
                }
                if (state == State::half_closed_local) {
                    if (policy::send_half_closed_local(f.type)) {
                        return Error{
                            H2Error::protocol,
                            false,
                            policy::recv_half_closed_remote.debug,
                        };
                    }
                }
                if (state == State::half_closed_remote) {
                    if (policy::has_end_stream_flag(f.type)) {
                        if (f.flag & frame::Flag::end_stream) {
                            state = State::closed;
                            // not perfectly closed
                            return no_error;  // no more handling needed for this header or data frame
                        }
                    }
                }
                return no_error;
            }

            constexpr Error on_recv(const frame::Frame& f) {
                if (id != f.id) {
                    return Error{
                        H2Error::internal,
                        false,
                        "stream routing is incorrect. library bug!!",
                    };
                }
                // RFC 9113 5.1. Stream States says:
                // Note that PRIORITY can be sent and received in any stream state.
                if (f.type == frame::Type::priority) {
                    return no_error;
                }
                if (state == State::idle) {
                    if (!policy::recv_idle(f.type)) {
                        return Error{
                            H2Error::protocol,
                            false,
                            policy::recv_idle.debug,
                        };
                    }
                    if (f.type == frame::Type::header) {
                        if (id.by_server()) {
                            return Error{
                                H2Error::protocol,
                                false,
                                "receiving HEADERS frame on idle with stream initiated by server",
                            };
                        }
                        state = State::open;
                    }
                }
                if (f.type == frame::Type::rst_stream) {
                    state = State::closed;
                    code = static_cast<const frame::RstStreamFrame&>(f).code;
                    perfect_closed = true;
                    return Error{H2Error(code), true};
                }
                if (state == State::reserved_local) {
                    if (!policy::recv_reserved_local(f.type)) {
                        return Error{
                            H2Error::protocol,
                            false,
                            policy::recv_reserved_local.debug,
                        };
                    }
                }
                if (state == State::reserved_remote) {
                    if (!policy::recv_reserved_remote(f.type)) {
                        return Error{
                            H2Error::protocol,
                            false,
                            policy::recv_reserved_remote.debug,
                        };
                    }
                    if (f.type == frame::Type::header) {
                        state = State::half_closed_local;
                    }
                }
                if (state == State::open) {
                    if (policy::has_end_stream_flag(f.type)) {
                        if (f.flag & frame::Flag::end_stream) {
                            state = State::half_closed_remote;
                            return no_error;  // no more handling needed for this header or data frame
                        }
                    }
                }
                if (state == State::half_closed_local) {
                    if (policy::has_end_stream_flag(f.type)) {
                        if (f.flag & frame::Flag::end_stream) {
                            state = State::closed;
                            perfect_closed = true;
                            return no_error;  // no more handling needed for this header or data frame
                        }
                    }
                }
                if (state == State::half_closed_remote) {
                    if (!policy::recv_half_closed_remote(f.type)) {
                        return Error{
                            H2Error::protocol,
                            false,
                            policy::recv_half_closed_remote.debug,
                        };
                    }
                }
                if (state == State::closed) {
                    if (perfect_closed) {
                        if (!policy::recv_closed(f.type)) {
                            return Error{
                                H2Error::protocol,
                                false,
                                policy::recv_closed.debug,
                            };
                        }
                    }
                }
                return no_error;
            }
        };

        struct FlowControlWindow {
           private:
            std::int64_t window = 0;

           public:
            constexpr bool update(std::uint32_t n) {
                if (exceeded(n)) {
                    return false;
                }
                window += n;
                return true;
            }

            constexpr void set_new_window(std::uint32_t old_initial_window, std::uint32_t new_initial_window) {
                window = new_initial_window - (old_initial_window - window);
            }

            constexpr bool consume(std::uint32_t n) {
                if (window < std::int64_t(n)) {
                    return false;
                }
                window -= n;
                return true;
            }

            constexpr std::uint64_t avail_size() const {
                return window < 0 ? 0 : window;
            }

            // RFC9113 6.9.1. The Flow-Control Window says:
            // A sender MUST NOT allow a flow-control window to exceed 2^31-1 octets.
            constexpr bool exceeded(std::uint32_t n) const {
                return window + n > 0x7fffffff;
            }
        };

        struct StreamNumDirState {
            // flow control window
            FlowControlWindow window;
            // previous processed frame type
            frame::Type preproced_type = frame::Type::settings;
            // previous processed is
            // - Type::header without Flag.end_headers
            // - Type::contiuous without Flag.end_headers
            bool on_continuous = false;

            constexpr StreamNumDirState() {
                window.set_new_window(0, 65535);
            }

            constexpr bool can_data(std::uint32_t data_size) const {
                return window.avail_size() >= data_size;
            }

            // RFC9113 6.9. WINDOW_UPDATE says:
            // Flow control only applies to frames that are identified
            // as being subject to flow control.
            // Of the frame types defined in this document,
            // this includes only DATA frames.
            // Frames that are exempt from flow control MUST be accepted
            // and processed, unless the receiver
            // is unable to assign resources to handling the frame.
            // A receiver MAY respond with a stream error (Section 5.4.2)
            // or connection error (Section 5.4.1) of type FLOW_CONTROL_ERROR
            // if it is unable to accept a frame.
            constexpr bool on_data(std::uint32_t data_size) {
                return window.consume(data_size);
            }

            constexpr bool on_window_update(std::int32_t increment) {
                return window.update(increment);
            }

            constexpr Error on_frame(const frame::Frame& f, StreamNumDirState& opposite) {
                if (on_continuous) {
                    if (f.type != frame::Type::continuation) {
                        return Error{H2Error::protocol};
                    }
                    if (f.flag & frame::Flag::end_headers) {
                        on_continuous = false;
                    }
                }
                else if (f.type == frame::Type::header ||
                         f.type == frame::Type::push_promise) {
                    if (!(f.flag & frame::Flag::end_headers)) {
                        on_continuous = true;
                    }
                }
                else if (f.type == frame::Type::data) {
                    // RFC9113 6.1. DATA says:
                    // The entire DATA frame payload is included in flow control,
                    // including the Pad Length and Padding fields if present.
                    if (!on_data(static_cast<const frame::DataFrame&>(f).data_len())) {
                        return Error{H2Error::flow_control};
                    }
                }
                else if (f.type == frame::Type::window_update) {
                    auto increment = static_cast<const frame::WindowUpdateFrame&>(f).increment;
                    if (!opposite.on_window_update(std::uint32_t(increment) & 0x7fffffff)) {
                        if (increment == 0) {
                            // RFC9113 6.9. WINDOW_UPDATE says:
                            // A receiver MUST treat the receipt of
                            // a WINDOW_UPDATE frame with a flow-control window
                            // increment of 0 as a stream error (Section 5.4.2) of
                            // type PROTOCOL_ERROR
                            return Error{H2Error::protocol, true};
                        }
                        return Error{H2Error::flow_control, true};
                    }
                }
                else if (f.type == frame::Type::rst_stream) {
                    // nothing to do
                }
                else if (is_connection_level(f.type) || f.type == frame::Type::continuation) {
                    return Error{H2Error::protocol};
                }
                else {
                    // ignore unknown type
                    return Error{H2Error::none};
                }
                preproced_type = f.type;
                return Error{H2Error::none};
            }
        };

        struct StreamNumState {
            // common stream state
            StreamNumCommonState com;
            // recv window(recv data,send window_update)
            StreamNumDirState recv;
            // send window(send data,recv window_update)
            StreamNumDirState send;

            constexpr ID id() const {
                return com.id;
            }

            constexpr State state() const {
                return com.state;
            }

            constexpr size_t sendable_size() const {
                return send.window.avail_size();
            }

            constexpr void maybe_close_by_higher_creation(ID max_server, ID max_client) {
                auto d = id();
                if (d.by_client()) {
                    if (d > max_client) {
                        com.maybe_close_by_higher_creation();
                    }
                }
                else {
                    if (d > max_server) {
                        com.maybe_close_by_higher_creation();
                    }
                }
            }

            constexpr Error on_recv_frame(const frame::Frame& f) {
                if (auto err = com.on_recv(f)) {
                    return err;
                }
                if (auto err = recv.on_frame(f, send)) {
                    return err;
                }
                return no_error;
            }

            // this method has rollback system
            // so that successfully updated state or not updated with error state exist only.
            constexpr Error on_send_frame(const frame::Frame& f) {
                if (auto err = com.on_send(f)) {
                    return err;
                }
                if (auto err = send.on_frame(f, recv)) {
                    return err;
                }
                return no_error;
            }

            constexpr Error can_send_frame(const frame::Frame& f) const {
                if (f.id != com.id) {
                    return Error{
                        H2Error::internal,
                        false,
                        "stream routing is incorrect. library bug!!",
                    };
                }
                if (send.on_continuous) {
                    if (f.type != frame::Type::continuation) {
                        return Error{H2Error::protocol, false, "expect CONTINUATION"};
                    }
                    return no_error;
                }
                if (auto err = com.can_send(f)) {
                    return err;
                }
                if (f.type == frame::Type::data) {
                    if (!send.can_data(static_cast<const frame::DataFrame&>(f).data_len())) {
                        return Error{H2Error::flow_control, true};
                    }
                }
                if (f.type == frame::Type::window_update) {
                    if (recv.window.exceeded(static_cast<const frame::WindowUpdateFrame&>(f).data_len())) {
                        return Error{H2Error::flow_control, true};
                    }
                }
                return no_error;
            }
        };

        /*
        // change_state_send changes http2 protocol state following sender role
        // state is represented at Section 5.1 of RFC7540
        // this function would changes s.err and on_cotinuous parameter
        // if state change succeeded returns true
        // otherwise returns false and library error code is set to s.err
        constexpr bool change_state_send(State& state, ErrCode& errs, bool& on_continuous, const frame::Frame& f) {
            using t = frame::Type;
            using l = frame::Flag;
            using a = State;
            if (on_continuous) {
                if (f.type != t::continuation) {
                    errs.err = http2_stream_state;
                    errs.h2err = H2Error::protocol;
                    errs.debug = "ContinousFrame expected but not";
                    return false;
                }
                if (f.flag & l::end_headers) {
                    on_continuous = false;
                }
                return true;
            }
            auto proc_flags = [&](a move_to) {
                if (f.type == t::data || f.type == t::header) {
                    if (f.flag & l::end_stream) {
                        state = move_to;
                    }
                }
                if (f.type == t::header && !(f.flag & l::end_headers)) {
                    on_continuous = true;
                }
            };

            if (state == a::idle) {
                if (f.type == t::header) {
                    state = a::open;
                }
                else if (f.type != t::priority) {
                    errs.err = http2_stream_state;
                    errs.h2err = H2Error::protocol;
                    errs.debug = "state is State.idle but Frame.type is not header or priority";
                    return false;
                }
            }
            if (state == a::reserved_local) {
                if (f.type == t::header) {
                    state = a::half_closed_remote;
                }
                else if (f.type != t::rst_stream && f.type != t::priority) {
                    errs.err = http2_stream_state;
                    errs.h2err = H2Error::protocol;
                    errs.debug = "state is State.reserved_local but Frame.type is not header, rst_stream or priority";
                    return false;
                }
            }
            if (state == a::reserved_remote) {
                if (f.type != t::rst_stream && f.type != t::window_update &&
                    f.type != t::priority) {
                    errs.err = http2_stream_state;
                    errs.h2err = H2Error::protocol;
                    errs.debug = "state is State.reserved_remote but Frame.type is not rst_stream, priority or window_update";
                    return false;
                }
            }
            bool changed_now = false;
            if (state == a::open) {
                proc_flags(a::half_closed_local);
                changed_now = true;
            }
            if (state == a::half_closed_local) {
                if (!changed_now && f.type != t::rst_stream && f.type != t::window_update &&
                    f.type != t::priority) {
                    errs.err = http2_stream_state;
                    errs.debug = "state is State.half_closed_local but Frame.type is not rst_stream, priority or window_update";
                    return false;
                }
            }
            if (state == a::half_closed_remote) {
                proc_flags(a::closed);
                changed_now = true;
            }
            if (f.type == t::rst_stream) {
                state = a::closed;
                changed_now = true;
            }
            if (state == a::closed) {
                if (!changed_now &&
                    f.type != t::priority &&
                    f.type != t::window_update) {
                    errs.err = http2_stream_state;
                    errs.h2err = H2Error::stream_closed;
                    errs.debug = "state is State.closed but Frame.type is not priority or window_update";
                    return false;
                }
            }
            return true;
        }

        // change_state_recv changes http2 protocol state following receiver role
        // state is represented at Section 5.1 of RFC7540
        // this function would changes s.err, s.h2err and on_cotinuous parameter
        // if state change succeeded returns true
        // otherwise returns false and library error code is set to s.err and s.h2err
        // if s.err was set and s.h2err is not set, that is library internal error below
        // - s.id != f.id
        constexpr bool change_state_recv(State& state, ErrCode& errs, bool& on_continuous, frame::Frame& f) {
            using t = frame::Type;
            using l = frame::Flag;
            using a = State;
            if (on_continuous) {
                if (f.type != t::continuation) {
                    errs.err = http2_stream_state;
                    errs.h2err = H2Error::protocol;
                    errs.debug = "Continuous Frame expected but not";
                    return false;
                }
                if (f.flag & l::end_headers) {
                    on_continuous = false;
                }
                return true;
            }
            auto proc_flags = [&](a move_to) {
                if (f.type == t::data || f.type == t::header) {
                    if (f.flag & l::end_stream) {
                        state = move_to;
                    }
                }
                if (f.type == t::header && !(f.flag & l::end_headers)) {
                    on_continuous = true;
                }
            };
            if (state == a::idle) {
                if (f.type == t::header) {
                    state = a::open;
                }
                else if (f.type != t::priority) {
                    errs.err = http2_stream_state;
                    errs.h2err = H2Error::protocol;
                    errs.debug = "state is State.idle but Frame.type is not header or priority";
                    return false;
                }
            }
            if (state == a::reserved_local) {
                if (f.type != t::priority && f.type != t::window_update && f.type != t::rst_stream) {
                    errs.err = http2_stream_state;
                    errs.h2err = H2Error::protocol;
                    errs.debug = "state is State.reserved_local but Frame.type is not rst_stream, priority or window_update";
                    return false;
                }
            }
            if (state == a::reserved_remote) {
                if (f.type == t::header) {
                    state = a::half_closed_local;
                }
                else if (f.type != t::rst_stream && f.type != t::priority) {
                    errs.err = http2_stream_state;
                    errs.h2err = H2Error::protocol;
                    errs.debug = "state is State.reserved_remote but Frame.type is not header rst_stream or priority";
                    return false;
                }
            }
            bool changed_now = false;
            if (state == a::open) {
                proc_flags(a::half_closed_remote);
                changed_now = true;
            }
            if (state == a::half_closed_local) {
                proc_flags(a::closed);
                changed_now = true;
            }
            if (state == a::half_closed_remote) {
                if (!changed_now && f.type != t::window_update && f.type != t::priority &&
                    f.type != t::rst_stream) {
                    errs.err = http2_stream_state;
                    errs.h2err = H2Error::protocol;
                    errs.debug = "state is State.half_closed_remote but Frame.type is not rst_stream, priority or window_update";
                    return false;
                }
            }
            if (f.type == t::rst_stream) {
                state = a::closed;
                changed_now = true;
            }
            if (state == a::closed) {
                if (!changed_now &&
                    f.type != t::priority &&
                    f.type != t::rst_stream &&
                    f.type != t::window_update) {
                    errs.err = http2_stream_state;
                    errs.h2err = H2Error::stream_closed;
                    errs.debug = "state is State.closed but Frame.type is not rst_stream, priority or window_update";
                    return false;
                }
            }
            return true;
        }*/

        struct ConnNumDirState {
            // previous read_frame/send_frame called id
            ID preproced_id = invalid_id;
            // previous read_frame/send_frame called type
            frame::Type preproced_type = frame::Type::invalid;
            // previous read_frame/send_frame called id excluding id 0
            ID preproced_stream_id = invalid_id;
            // previous read_frame/send_frame called type excluding id 0
            frame::Type preproced_stream_type = frame::Type::invalid;
            // predefine settings
            setting::PredefinedSettings settings;
            // flow control window
            FlowControlWindow window;

            bool on_continuation = false;

            constexpr bool can_data(std::uint32_t data_size) const {
                return window.avail_size() >= data_size;
            }

            constexpr bool on_data(std::uint32_t data_size) {
                return window.consume(data_size);
            }

            constexpr bool on_window_update(std::int32_t increment) {
                return window.update(increment);
            }

            constexpr Error on_settings(const frame::SettingsFrame& s) {
                if (s.flag & frame::Flag::ack) {
                    return no_error;  // ignore
                }
                Error err;
                auto res = s.visit([&](std::uint16_t key, std::uint32_t value) {
                    if (k(setting::SettingKey::table_size) == key) {
                        settings.header_table_size = value;
                    }
                    else if (k(setting::SettingKey::enable_push) == key) {
                        if (value != 1 && value != 0) {
                            err = Error{H2Error::protocol};
                            return false;
                        }
                        settings.enable_push = value;
                    }
                    else if (k(setting::SettingKey::max_concurrent) == key) {
                        settings.max_concurrent_stream = value;
                    }
                    else if (k(setting::SettingKey::initial_windows_size) == key) {
                        if (value > 0x7fffffff) {
                            err = Error{H2Error::flow_control};
                            return false;
                        }
                        settings.initial_window_size = value;
                    }
                    else if (k(setting::SettingKey::max_frame_size) == key) {
                        settings.max_frame_size = value;
                    }
                    else if (k(setting::SettingKey::header_list_size) == key) {
                        settings.max_header_list_size = value;
                    }
                    return true;
                });
                if (!res) {
                    return Error{H2Error::internal};
                }
                return err;
            }

            constexpr Error on_frame(const frame::Frame& f, ConnNumDirState& opposite) {
                if (f.len > settings.max_frame_size) {
                    return Error{H2Error::frame_size};
                }
                if (on_continuation) {
                    if (f.type != frame::Type::continuation) {
                        return Error{H2Error::protocol, false, "expect CONTINUATION but not"};
                    }
                    if (f.id != preproced_stream_id) {
                        return Error{H2Error::protocol, false, "expect same id previous received but not"};
                    }
                    if (f.flag & frame::Flag::end_headers) {
                        on_continuation = false;
                    }
                }
                preproced_id = f.id;
                preproced_type = f.type;
                if (!f.id.is_conn()) {
                    preproced_stream_id = f.id;
                    preproced_stream_type = f.type;
                }
                if (f.type == frame::Type::data) {
                    if (!on_data(static_cast<const frame::DataFrame&>(f).data_len())) {
                        return Error{H2Error::flow_control};
                    }
                }
                else if (f.type == frame::Type::window_update && f.id == 0) {
                    auto increment = static_cast<const frame::WindowUpdateFrame&>(f).increment;
                    if (!opposite.on_window_update(std::uint32_t(increment) & 0x7fffffff)) {
                        if (increment == 0) {
                            // RFC9113 6.9. WINDOW_UPDATE says:
                            // A receiver MUST treat the receipt of
                            // a WINDOW_UPDATE frame with a flow-control window
                            // increment of 0 as a stream error (Section 5.4.2) of
                            // type PROTOCOL_ERROR; errors on the connection flow-control window
                            // MUST be treated as a connection error (Section 5.4.1).
                            return Error{H2Error::protocol};
                        }
                        return Error{H2Error::flow_control};
                    }
                }
                else if (f.type == frame::Type::settings) {
                    return opposite.on_settings(static_cast<const frame::SettingsFrame&>(f));
                }
                else if (f.type == frame::Type::header || f.type == frame::Type::push_promise) {
                    if (!(f.flag & frame::Flag::end_headers)) {
                        on_continuation = true;
                    }
                }
                return no_error;  // nothing to check
            }
        };

        struct CloseState {
            // error code by GOAWAY
            std::uint32_t code = 0;
            bool closed = false;
            ID processed_id = 0;

            void on_goaway(const frame::GoAwayFrame& g) {
                code = g.code;
                processed_id = g.processed_id;
                closed = true;
            }
        };

        struct ConnNumState {
            CloseState close;
            ID max_client_id = invalid_id;
            ID max_server_id = invalid_id;
            ID max_closed_client_id = invalid_id;
            ID max_closed_server_id = invalid_id;

            ID max_processed = invalid_id;
            void on_recv_processed(ID id) {
            }
            // recv
            ConnNumDirState recv;
            // send preproced_id/local settings
            ConnNumDirState send;
            // is server mode
            bool is_server = false;

            constexpr setting::PredefinedSettings send_settings() const {
                return send.settings;
            }

            constexpr Error on_recv_frame(const frame::Frame& f) {
                if (auto err = recv.on_frame(f, send)) {
                    return err;
                }
                if (f.type == frame::Type::goaway) {
                    close.on_goaway(static_cast<const frame::GoAwayFrame&>(f));
                }
                return no_error;
            }

            constexpr Error on_send_frame(const frame::Frame& f) {
                if (f.type == frame::Type::push_promise) {
                    if (!send.settings.enable_push) {
                        return Error{H2Error::protocol, false, "push disabled"};
                    }
                }
                if (auto err = send.on_frame(f, recv)) {
                    return err;
                }
                if (f.type == frame::Type::goaway) {
                    close.on_goaway(static_cast<const frame::GoAwayFrame&>(f));
                }
                return no_error;
            }

            constexpr void on_stream_frame_processed(StreamNumState& s) {
                if (s.id().by_server()) {
                    if (max_server_id < s.id()) {
                        max_server_id = s.id();
                    }
                    if (s.state() == State::closed && max_closed_server_id < s.id()) {
                        max_server_id = s.id();
                    }
                }
                else {  // by client
                    if (max_client_id < s.id()) {
                        max_client_id = s.id();
                    }
                    if (s.state() == State::closed && max_closed_client_id < s.id()) {
                        max_client_id = s.id();
                    }
                }
            }

            constexpr void maybe_closed_by_higher_creation(StreamNumState& s) {
                s.maybe_close_by_higher_creation(max_server_id, max_client_id);
            }

            constexpr Error on_recv_stream_frame(const frame::Frame& f, StreamNumState& s) {
                if (f.id != s.id()) {
                    return Error{H2Error::internal, false, "stream routing is incorrect. library bug!!"};
                }
                if (f.id.is_conn() || !f.id.valid()) {
                    return Error{H2Error::internal, false, "invalid stream id. library bug!!"};
                }
                maybe_closed_by_higher_creation(s);
                if (auto err = on_recv_frame(f)) {
                    return err;
                }
                if (auto err = s.on_recv_frame(f)) {
                    return err;
                }
                on_stream_frame_processed(s);
                return no_error;
            }

            constexpr Error on_send_stream_frame(const frame::Frame& f, StreamNumState& s) {
                if (f.id != s.id()) {
                    return Error{H2Error::internal, false, "stream routing is incorrect. library bug!!"};
                }
                if (f.id.is_conn() || !f.id.valid()) {
                    return Error{H2Error::internal, false, "invalid stream id. library bug!!"};
                }
                s.maybe_close_by_higher_creation(max_server_id, max_client_id);
                if (auto err = on_send_frame(f)) {
                    return err;
                }
                if (auto err = s.on_send_frame(f)) {
                    return err;
                }
                on_stream_frame_processed(s);
                return no_error;
            }

            constexpr Error can_send_stream_frame(const frame::Frame& f, const StreamNumState& s) {
                if (f.id != s.id()) {
                    return Error{H2Error::internal, false, "stream routing is incorrect. library bug!!"};
                }
                if (f.id.is_conn() || !f.id.valid()) {
                    return Error{H2Error::internal, false, "invalid stream id. library bug!!"};
                }
                if (send.settings.max_frame_size < frame::frame_len(f)) {
                    return Error{H2Error::frame_size};
                }
                if (send.on_continuation) {
                    if (f.type != frame::Type::continuation) {
                        return Error{H2Error::protocol, false, "expect CONTINUATION"};
                    }
                    if (f.id != send.preproced_stream_id) {
                        return Error{H2Error::protocol, false, "expect same id sent before"};
                    }
                }
                if (f.type == frame::Type::data) {
                    if (!send.can_data(static_cast<const frame::DataFrame&>(f).data_len())) {
                        return Error{H2Error::flow_control, false};
                    }
                }
                return s.can_send_frame(f);
            }
        };

        constexpr size_t get_sendable_size(ConnNumState& c, StreamNumState& s) {
            auto cn = c.send.window.avail_size();
            auto sn = s.send.window.avail_size();
            if (cn < sn) {
                return cn;
            }
            else {
                return sn;
            }
        }

        /*
        constexpr int get_next_id(const ConnNumState& s) {
            if (s.is_server) {
                return s.max_issued_id % 2 == 0 ? s.max_issued_id + 2 : s.max_issued_id + 1;
            }
            else {
                return s.max_issued_id % 2 == 0 ? s.max_issued_id + 1 : s.max_issued_id + 2;
            }
        }

        constexpr bool update_max_issued_id(ConnNumState& s, int new_id) {
            if (new_id > s.max_issued_id) {
                s.max_issued_id = new_id;
                return true;
            }
            return false;
        }

        constexpr bool update_max_closed_id(ConnNumState& s, int new_id) {
            if (new_id > s.max_closed_id) {
                s.max_closed_id = new_id;
                return true;
            }
            return false;
        }

        // has_frame_state_error  detects error state between stream state and frame type.
        // this function would not detect error that parse_frame/render_frame function change_state_* function detects.
        // this behaviour is to follow DRY principle.
        constexpr bool has_frame_state_error(
            bool send,
            h2set::PredefinedSettings& ps, State state, ErrCode& errs, frame::Frame& f) {
            using t = frame::Type;
            using l = frame::Flag;
            using a = State;
            using e = H2Error;
            auto st = state;
            if (f.type == t::data) {
                if (send) {
                    if (st != a::open && st != a::half_closed_remote) {
                        errs.strm = true;
                        errs.err = http2_stream_state;
                        errs.h2err = e::stream_closed;
                        errs.debug = "DataFrame is not on State.open or State.half_closed_remote";
                        return true;
                    }
                }
                else {
                    if (st != a::open && st != a::half_closed_local) {
                        errs.strm = true;
                        errs.err = http2_stream_state;
                        errs.h2err = e::stream_closed;
                        errs.debug = "DataFrame is not on State.open or State.half_closed_local";
                        return true;
                    }
                }
            }
            if (f.type == t::header) {
                // nothing to do
            }
            if (f.type == t::priority) {
                // nothing to do
            }
            if (f.type == t::rst_stream) {
                if (st == a::idle) {
                    errs.err = http2_stream_state;
                    errs.h2err = e::protocol;
                    errs.debug = "RstStreamFrame on State.idle";
                    return true;
                }
            }
            if (f.type == t::settings) {
                // nothing to do
            }
            if (f.type == t::push_promise) {
                if (!ps.enable_push) {
                    errs.err = http2_stream_state;
                    errs.h2err = e::protocol;
                    errs.debug = "PushPromiseFrame when SETTINGS_ENABLE_PUSH is disabled";
                    return true;
                }
            }
            if (f.type == t::ping) {
                // nothing to do
            }
            if (f.type == t::goaway) {
                // nothing to do
            }
            if (f.type == t::window_update) {
                // nothing to do
            }
            return false;
        }

        constexpr bool update_window(ConnNumState& c, StreamNumState& s, frame::Frame& f, bool on_send) {
            StreamNumDirState *sdir = nullptr, *s0dir = nullptr,
                              *rdir = nullptr, *r0dir = nullptr;
            if (on_send) {
                sdir = &s.send;
                s0dir = &c.s0.send;  // sending data -> effects to send parameter
                rdir = &s.recv;
                r0dir = &c.s0.recv;  // sending window_update -> effects to recv parameter
            }
            else {
                sdir = &s.recv;
                s0dir = &c.s0.recv;  // receiving data  -> effects to recv parameter
                rdir = &s.send;
                r0dir = &c.s0.send;  // receiving winodw_update -> effects to send parameter
            }
            if (f.type == frame::Type::data) {
                if (s.com.id != f.id) {
                    s.com.errs.err = http2_invalid_id;
                    s.com.errs.debug = "s.com.id != f.id on DataFrame";
                    return false;
                }
                sdir->window.consume(f.len);
                s0dir->window.consume(f.len);
            }
            else if (f.type == frame::Type::window_update) {
                auto& w = static_cast<frame::WindowUpdateFrame&>(f);
                if (w.id == 0) {
                    r0dir->window.update(w.increment);
                }
                else if (w.id == s.com.id) {
                    rdir->window.update(w.increment);
                }
                else {
                    s.com.errs.err = http2_invalid_id;
                    s.com.errs.debug = "f.id != s.com.id&& f.id != 0 on WindowUpdateFrame";
                    return false;
                }
            }
            return true;
        }

        constexpr void set_new_window(std::int64_t& window, std::uint32_t old_initial_window, std::uint32_t new_initial_window) {
            window = new_initial_window - (old_initial_window - window);
        }*

        struct ConnDirState {
            using strpair = std::pair<flex_storage, flex_storage>;
            using Table = slib::deque<strpair>;
            flex_storage settings;
            Table table;
        };

        struct ConnState {
            // number states
            ConnNumState state;
            // encoder table/local settings
            ConnDirState send;
            // decoder table/remote settings
            ConnDirState recv;
        };

        constexpr void check_idle_stream(ConnNumState& c, StreamNumState& s) {
            if (s.com.state == State::idle) {
                if (c.max_server_id > s.com.id) {
                    s.com.state = State::closed;  // implicit closed
                }
            }
        }

        constexpr size_t get_sendable_size(StreamNumDirState& c, StreamNumDirState& s) {
            auto cn = c.window.avail_size();
            auto sn = s.window.avail_size();
            if (cn < sn) {
                return cn;
            }
            else {
                return sn;
            }
        }

        constexpr bool check_data_window(const FlowControlWindow& cwindow, const FlowControlWindow& swindow, ErrCode& errs, frame::Frame& f) {
            if (f.type == frame::Type::data) {
                if (cwindow.avail_size() < f.len || swindow.avail_size() < f.len) {
                    errs.err = http2_should_reduce_data;
                    errs.h2err = H2Error::enhance_your_clam;
                    errs.debug = "too large data length against window size on DataFrame";
                    return false;
                }
            }
            return true;
        }

        constexpr bool check_continous(int preproced_id, ErrCode& errs, frame::Frame& f) {
            if (preproced_id != f.id) {
                errs.err = http2_invalid_header_blocks;
                errs.h2err = H2Error::protocol;
                errs.debug = "ContinousFrame expected but not";
                return false;
            }
            return true;
        }
        *
        // recv_frame updates conn state and stream state
        // parse_frame -> has_frame_state_error -> recv_frame
        // is better way to detect error
        constexpr bool recv_frame(ConnNumState& c, StreamNumState& s, frame::Frame& f) {
            if (c.s0.recv.on_continuous) {
                if (!check_continous(c.recv.preproced_id, s.com.errs, f)) {
                    return false;
                }
            }
            update_window(c, s, f, false);
            c.send.preproced_id = f.id;
            c.send.preproced_type = f.type;
            if (f.id != 0) {
                check_idle_stream(c, s);
                if (!check_data_window(c.s0.recv.window, s.recv.window, s.com.errs, f)) {
                    return false;  // detect too large data frame
                }
                if (has_frame_state_error(false, c.send.settings, s.com.state, s.com.errs, f)) {
                    return false;
                }
                auto res = change_state_recv(s.com.state, s.com.errs, s.recv.on_continuous, f);
                if (!res) {
                    return false;
                }
                s.recv.preproced_type = f.type;
                c.recv.preproced_stream_id = f.id;
                c.s0.recv.on_continuous = s.recv.on_continuous;
                update_max_issued_id(c, f.id);
                if (s.com.state == State::closed) {
                    update_max_closed_id(c, f.id);
                }
            }
            return true;
        }

        constexpr bool send_frame(ConnNumState& c, StreamNumState& s, frame::Frame& f) {
            if (c.s0.send.on_continuous) {
                if (c.send.preproced_id != f.id) {
                    s.com.errs.err = http2_invalid_header_blocks;
                    s.com.errs.h2err = H2Error::protocol;
                    s.com.errs.debug = "ContinousFrame expected but not";
                    return false;
                }
            }
            update_window(c, s, f, true);
            c.send.preproced_id = f.id;
            c.send.preproced_type = f.type;
            if (f.id != 0) {
                check_idle_stream(c, s);
                if (!check_data_window(c.s0.send.window, s.send.window, s.com.errs, f)) {
                    return false;
                }
                if (has_frame_state_error(true, c.recv.settings, s.com.state, s.com.errs, f)) {
                    return false;
                }
                auto res = change_state_send(s.com.state, s.com.errs, s.send.on_continuous, f);
                if (!res) {
                    return false;
                }
                s.send.preproced_type = f.type;
                c.send.preproced_stream_id = f.id;
                c.send.preproced_stream_type = f.type;
                c.s0.send.on_continuous = s.send.on_continuous;
                update_max_issued_id(c, f.id);
                if (s.com.state == State::closed) {
                    update_max_closed_id(c, f.id);
                }
            }
            return true;
        }*/

    }  // namespace fnet::http2::stream
}  // namespace utils
