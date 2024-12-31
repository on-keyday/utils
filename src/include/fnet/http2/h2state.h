/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// h2state - http2 state machines
#pragma once
#include "h2frame.h"
#include "h2frame.h"
#include "h2err.h"
#include "h2settings.h"
#include "../std/deque.h"
#include "../dll/allocator.h"
#include "../storage.h"
#include "stream_id.h"
#include "../std/hash_map.h"
#include <wrap/light/enum.h>
#include "../http1/state.h"

namespace futils {
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

        BEGIN_ENUM_STRING_MSG(State, state_name)
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
        struct FlowControlWindow {
           private:
            std::int64_t window = 65535;

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

        struct StreamCommonState {
            // stream id
            ID id = invalid_id;
            // error code
            std::uint32_t code = 0;
            // stream state machine
            State state = State::idle;
            // no need handling exclude PRIORITY
            bool perfect_closed = false;
            http1::HTTPState recv_state = http1::HTTPState::init;

            constexpr StreamCommonState(ID id)
                : id(id) {}

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
                    if (!policy::send_idle(f.type)) {
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

        enum class ContinuousState : byte {
            none,
            on_header,
            on_push_promise,
        };

        struct StreamDirState {
           private:
            // flow control window
            FlowControlWindow window;
            // previous processed frame type
            frame::Type preproced_type = frame::Type::settings;
            // previous processed is
            // - Type::header without Flag.end_headers
            // - Type::push_promise without Flag.end_headers
            // - Type::contiuous without Flag.end_headers
            ContinuousState on_continuous = ContinuousState::none;

           public:
            constexpr StreamDirState() {
                window.set_new_window(0, 65535);
            }

            constexpr ContinuousState continuous_state() const {
                return on_continuous;
            }

            constexpr bool can_data(std::uint32_t data_size) const {
                return window.avail_size() >= data_size;
            }

            constexpr size_t window_available_size() const {
                return window.avail_size();
            }

            constexpr bool window_exceeded(std::uint32_t n) const {
                return window.exceeded(n);
            }

            constexpr void set_new_window(std::uint32_t old_initial_window, std::uint32_t new_initial_window) {
                window.set_new_window(old_initial_window, new_initial_window);
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

            constexpr Error on_frame(const frame::Frame& f, StreamDirState& opposite) {
                if (on_continuous != ContinuousState::none) {
                    if (f.type != frame::Type::continuation) {
                        return Error{H2Error::protocol};
                    }
                    if (f.flag & frame::Flag::end_headers) {
                        on_continuous = ContinuousState::none;
                    }
                }
                else if (f.type == frame::Type::header ||
                         f.type == frame::Type::push_promise) {
                    if (!(f.flag & frame::Flag::end_headers)) {
                        on_continuous = f.type == frame::Type::header ? ContinuousState::on_header : ContinuousState::on_push_promise;
                    }
                }
                else if (f.type == frame::Type::data) {
                    // RFC9113 6.1. DATA says:
                    // The entire DATA frame payload is included in flow control,
                    // including the Pad Length and Padding fields if present.
                    if (!on_data(static_cast<const frame::DataFrame&>(f).data_len())) {
                        return Error{H2Error::flow_control, false, "DATA frame exceeded flow control limit"};
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
                            return Error{H2Error::protocol, true, "WINDOW not updated"};
                        }
                        return Error{H2Error::flow_control, true, "WINDOW update exceed flow control limit"};
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

        struct StreamState {
           private:
            // common stream state
            StreamCommonState com;
            // peer settings (recv data,send window_update)
            StreamDirState peer;
            // local settings (send data,recv window_update)
            StreamDirState local;

           public:
            constexpr StreamState(ID id)
                : com(id) {}

            constexpr ContinuousState recv_continuous_state() const {
                return local.continuous_state();
            }

            constexpr void set_recv_state(http1::HTTPState state) {
                com.recv_state = state;
            }

            constexpr http1::HTTPState recv_state() const {
                return com.recv_state;
            }

            constexpr ID id() const {
                return com.id;
            }

            constexpr State state() const {
                return com.state;
            }

            constexpr size_t sendable_size() const {
                return peer.window_available_size();
            }

            constexpr void maybe_close_by_higher_creation(ID max_server, ID max_client) {
                auto d = id();
                if (d.by_client()) {
                    if (d < max_client) {
                        com.maybe_close_by_higher_creation();
                    }
                }
                else {
                    if (d < max_server) {
                        com.maybe_close_by_higher_creation();
                    }
                }
            }

            constexpr Error on_recv_frame(const frame::Frame& f) {
                if (auto err = com.on_recv(f)) {
                    return err;
                }
                if (auto err = local.on_frame(f, peer)) {
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
                if (auto err = peer.on_frame(f, local)) {
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
                if (peer.continuous_state() != ContinuousState::none) {
                    if (f.type != frame::Type::continuation) {
                        return Error{H2Error::protocol, false, "expect CONTINUATION"};
                    }
                    return no_error;
                }
                if (auto err = com.can_send(f)) {
                    return err;
                }
                if (f.type == frame::Type::data) {
                    if (!peer.can_data(static_cast<const frame::DataFrame&>(f).data_len())) {
                        return Error{H2Error::flow_control, true, "cannot send DATA frame by stream flow control limit"};
                    }
                }
                if (f.type == frame::Type::window_update) {
                    if (peer.window_exceeded(static_cast<const frame::WindowUpdateFrame&>(f).data_len())) {
                        return Error{H2Error::flow_control, true, "cannot send WINDOW_UPDATE frame"};
                    }
                }
                return no_error;
            }

            constexpr void set_new_local_window(std::uint32_t old_initial_window, std::uint32_t new_initial_window) {
                local.set_new_window(old_initial_window, new_initial_window);
            }

            constexpr void set_new_peer_window(std::uint32_t old_initial_window, std::uint32_t new_initial_window) {
                peer.set_new_window(old_initial_window, new_initial_window);
            }
        };

        struct ConnDirState {
            // previous read_frame/send_frame called id
            ID preproced_id = invalid_id;
            // previous read_frame/send_frame called id excluding id 0
            ID preproced_stream_id = invalid_id;
            // predefine settings
            setting::PredefinedSettings settings;
            // flow control window
            FlowControlWindow window;
            // previous read_frame/send_frame called type excluding id 0
            frame::Type preproced_stream_type = frame::Type::invalid;
            // previous read_frame/send_frame called type
            frame::Type preproced_type = frame::Type::invalid;

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

            constexpr Error on_settings(const frame::SettingsFrame& s, auto&& on_initial_window_update) {
                if (s.flag & frame::Flag::ack) {
                    return no_error;  // ignore
                }
                Error err;
                auto res = s.visit([&](std::uint16_t key, std::uint32_t value) {
                    if (k(setting::SettingKey::max_header_table_size) == key) {
                        settings.max_header_table_size = value;
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
                            err = Error{H2Error::flow_control, false, "settings initial_window_size exceeds 2^31-1"};
                            return false;
                        }
                        auto old_initial = settings.initial_window_size;
                        settings.initial_window_size = value;
                        window.set_new_window(old_initial, value);
                        on_initial_window_update(old_initial, value);  // for streams flow window update
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
                    return Error{H2Error::internal, false, "cannot read settings frame"};
                }
                return err;
            }

            constexpr Error on_frame(const frame::Frame& f, ConnDirState& opposite, auto&& on_initial_window_updated) {
                if (f.len > settings.max_frame_size) {
                    return Error{H2Error::frame_size, false, "frame size exceeded max_frame_size"};
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
                        return Error{H2Error::flow_control, false, "DATA frame exceeded flow control limit"};
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
                        return Error{H2Error::flow_control, false, "WINDOW_UPDATE frame exceeded flow control limit"};
                    }
                }
                else if (f.type == frame::Type::settings) {
                    return opposite.on_settings(static_cast<const frame::SettingsFrame&>(f), on_initial_window_updated);
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
            ID processed_id = 0;
            bool closed = false;

            void on_goaway(const frame::GoAwayFrame& g) {
                code = g.code;
                processed_id = g.processed_id;
                closed = true;
            }
        };

        struct ConnState {
           private:
            CloseState close;
            ID max_client_id = invalid_id;
            ID max_server_id = invalid_id;
            ID max_closed_client_id = invalid_id;
            ID max_closed_server_id = invalid_id;

            ID max_processed = invalid_id;
            ID issue_id = invalid_id;

            // recv
            ConnDirState peer;
            // send preproced_id/local settings
            ConnDirState local;
            // is server mode
            bool is_server_ = false;

           public:
            constexpr ConnState(bool server)
                : is_server_(server) {}

            constexpr bool is_server() const noexcept {
                return is_server_;
            }

            constexpr bool is_closed() const noexcept {
                return close.closed;
            }

            void on_recv_processed(ID id) {
            }

            constexpr setting::PredefinedSettings local_settings() const {
                return local.settings;
            }

            constexpr setting::PredefinedSettings peer_settings() const {
                return peer.settings;
            }

            constexpr Error on_recv_frame(const frame::Frame& f, auto&& on_initial_window_updated) {
                if (f.type == frame::Type::push_promise) {
                    if (!local.settings.enable_push) {
                        return Error{H2Error::protocol, false, "push disabled"};
                    }
                }
                if (auto err = local.on_frame(f, peer, on_initial_window_updated)) {
                    return err;
                }
                if (f.type == frame::Type::goaway) {
                    close.on_goaway(static_cast<const frame::GoAwayFrame&>(f));
                }
                return no_error;
            }

            constexpr Error on_send_frame(const frame::Frame& f, auto&& on_initial_window_updated) {
                if (f.type == frame::Type::push_promise) {
                    if (!peer.settings.enable_push) {
                        return Error{H2Error::protocol, false, "push disabled"};
                    }
                }
                if (auto err = peer.on_frame(f, local, on_initial_window_updated)) {
                    return err;
                }
                if (f.type == frame::Type::goaway) {
                    close.on_goaway(static_cast<const frame::GoAwayFrame&>(f));
                }
                return no_error;
            }

            constexpr void on_stream_frame_processed(StreamState& s) {
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

            constexpr void on_stream_created(StreamState& s) {
                maybe_closed_by_higher_creation(s);
            }

           private:
            constexpr void maybe_closed_by_higher_creation(StreamState& s) {
                s.maybe_close_by_higher_creation(max_server_id, max_client_id);
            }

           public:
            constexpr Error on_recv_stream_frame(const frame::Frame& f, StreamState& s) {
                if (f.id != s.id()) {
                    return Error{H2Error::internal, false, "stream routing is incorrect. library bug!!"};
                }
                if (f.id.is_conn() || !f.id.valid()) {
                    return Error{H2Error::internal, false, "invalid stream id. library bug!!"};
                }
                maybe_closed_by_higher_creation(s);
                // at here, SETTINGS frame must not arrive
                if (auto err = on_recv_frame(f, [](auto&&...) {})) {
                    return err;
                }
                if (auto err = s.on_recv_frame(f)) {
                    return err;
                }
                on_stream_frame_processed(s);
                return no_error;
            }

            constexpr Error on_send_stream_frame(const frame::Frame& f, StreamState& s) {
                if (f.id != s.id()) {
                    return Error{H2Error::internal, false, "stream routing is incorrect. library bug!!"};
                }
                if (f.id.is_conn() || !f.id.valid()) {
                    return Error{H2Error::internal, false, "invalid stream id. library bug!!"};
                }
                maybe_closed_by_higher_creation(s);
                // at here, SETTINGS frame must not arrive
                if (auto err = on_send_frame(f, [](auto&&...) {})) {
                    return err;
                }
                if (auto err = s.on_send_frame(f)) {
                    return err;
                }
                on_stream_frame_processed(s);
                return no_error;
            }

            constexpr ID new_stream() {
                if (!issue_id.valid()) {
                    issue_id = is_server_ ? server_first : client_first;
                    return issue_id;
                }
                auto next = issue_id.next();
                if (!next.valid()) {
                    return invalid_id;
                }
                issue_id = next;
                return issue_id;
            }

            constexpr Error can_send_stream_frame(const frame::Frame& f, const StreamState& s) {
                if (f.id != s.id()) {
                    return Error{H2Error::internal, false, "stream routing is incorrect. library bug!!"};
                }
                if (f.id.is_conn() || !f.id.valid()) {
                    return Error{H2Error::internal, false, "invalid stream id. library bug!!"};
                }
                if (peer.settings.max_frame_size < frame::frame_len(f)) {
                    return Error{H2Error::frame_size, false, "frame size exceeded max_frame_size"};
                }
                if (peer.on_continuation) {
                    if (f.type != frame::Type::continuation) {
                        return Error{H2Error::protocol, false, "expect CONTINUATION"};
                    }
                    if (f.id != peer.preproced_stream_id) {
                        return Error{H2Error::protocol, false, "expect same id sent before"};
                    }
                }
                if (f.type == frame::Type::data) {
                    if (!peer.can_data(static_cast<const frame::DataFrame&>(f).data_len())) {
                        return Error{H2Error::flow_control, false, "cannot send DATA frame by connection flow limit"};
                    }
                }
                if (f.type == frame::Type::push_promise) {
                    if (!peer.settings.enable_push) {
                        return Error{H2Error::protocol, false, "push disabled"};
                    }
                }
                return s.can_send_frame(f);
            }

            ID get_max_processed() {
                return max_processed;
            }
        };

        constexpr size_t get_sendable_size(ConnState& c, StreamState& s) {
            return 0;  // TODO(on-keyday): implement or remove
        }

        namespace test {
            constexpr auto sizeof_conn_state = sizeof(ConnState);
            constexpr auto sizeof_stream_state = sizeof(StreamState);

            struct States {
                ConnState conn;
                StreamState stream_1;
                StreamState stream_2;
                StreamState stream_3;
                StreamState stream_4;
                constexpr States(bool server, ID id1, ID id2, ID id3, ID id4)
                    : conn(server),
                      stream_1(id1),
                      stream_2(id2),
                      stream_3(id3),
                      stream_4(id4) {}
                template <size_t n>
                constexpr StreamState& stream() {
#define SWITCH()           \
    if constexpr (false) { \
    }
#define CASE(N) else if constexpr (n == N) return stream_##N;
                    SWITCH()
                    CASE(1)
                    CASE(2)
                    CASE(3)
                    CASE(4)
                    else {
                        throw "why";
                    }

#undef SWITCH
#undef CASE
                }
            };

            constexpr auto make_states(bool is_server, ID first) {
                States client{is_server, first, first.next(),
                              first.next().next(), first.next().next().next()};
                return client;
            }

            template <size_t size>
            constexpr byte virtual_buffer[size]{};

            constexpr auto on_err(const char* msg, const char* debug = nullptr) {
                if (std::is_constant_evaluated()) {
                    throw "cannot open";
                }
                return;
            }

            template <size_t n, size_t err_step = 0>
            constexpr void apply_stream_frames(const frame::Frame& f, States& sender, States& receiver, State expect_sender, State expect_receiver) {
                if (sender.stream<n>().state() == State::idle) {
                    sender.conn.on_stream_created(sender.stream<n>());
                }
                // should not be error
                if (auto err = sender.conn.can_send_stream_frame(f, sender.stream<n>())) {
                    if (err_step == 1) {
                        return;
                    }
                    on_err(error_msg(err.code), err.debug);
                }
                if (err_step == 1) {
                    on_err("expect error at step 1 but no error");
                }
                if (auto err = sender.conn.on_send_stream_frame(f, sender.stream<n>())) {
                    if (err_step == 2) {
                        return;
                    }
                    on_err(error_msg(err.code), err.debug);
                }
                if (err_step == 2) {
                    on_err("expect error at step 2 but no error");
                }
                if (sender.stream<n>().state() != expect_sender) {
                    if (err_step == 3) {
                        return;
                    }
                    on_err(state_name(sender.stream<n>().state()));
                }
                if (err_step == 3) {
                    on_err("expect error at step 3 but no error");
                }
                if (receiver.stream<n>().state() == State::idle) {
                    receiver.conn.on_stream_created(receiver.stream<n>());
                }
                if (auto err = receiver.conn.on_recv_stream_frame(f, receiver.stream<n>())) {
                    if (err_step == 4) {
                        return;
                    }
                    on_err(error_msg(err.code), err.debug);
                }
                if (err_step == 4) {
                    on_err("expect error at step 4 but no error");
                }
                if (receiver.stream<n>().state() != expect_receiver) {
                    if (err_step == 5) {
                        return;
                    }
                    on_err(state_name(receiver.stream<n>().state()));
                }
                if (err_step == 5) {
                    on_err("expect error at step 5 but no error");
                }
            }

            template <size_t err_step = 0>
            constexpr void apply_conn_frames(const frame::Frame& f, States& sender, States& receiver) {
                if (auto err = sender.conn.on_send_frame(f, [&](auto&&...) {})) {
                    if (err_step == 1) {
                        return;
                    }
                    on_err(error_msg(err.code), err.debug);
                }
                if (err_step == 1) {
                    on_err("expect error at step 1 but no error");
                }
                if (auto err = receiver.conn.on_recv_frame(f, [&](auto&&...) {})) {
                    if (err_step == 2) {
                        return;
                    }
                    on_err(error_msg(err.code), err.debug);
                }
                if (err_step == 2) {
                    on_err("expect error at step 4 but no error");
                }
            }

            constexpr bool check_open() {
                auto client = make_states(false, client_first);
                auto server = make_states(true, client_first);
                frame::HeaderFrame head;
                head.id = client.stream_1.id();
                constexpr auto default_frame_size = setting::PredefinedSettings{}.max_frame_size;
                constexpr auto default_flow = setting::PredefinedSettings{}.initial_window_size;
                head.data = virtual_buffer<default_frame_size - 9>;
                apply_stream_frames<1>(head, client, server, State::open, State::open);
                // should detect error at first step (can_send_stream_frame)
                apply_stream_frames<1, 1>(head, client, server, State::open, State::open);
                frame::ContinuationFrame cont;
                cont.id = client.stream_1.id();
                cont.data = virtual_buffer<100>;
                cont.flag |= frame::Flag::end_headers;
                apply_stream_frames<1>(cont, client, server, State::open, State::open);
                frame::DataFrame data;
                data.id = client.stream_1.id();
                data.data = virtual_buffer<default_frame_size - 9>;
                for (auto i = 0; i < 4; i++) {
                    apply_stream_frames<1>(data, client, server, State::open, State::open);
                }
                // should detect error at first step (can_send_stream_frame)
                data.flag |= frame::Flag::end_stream;
                apply_stream_frames<1, 1>(data, client, server, State::closed, State::closed);
                frame::WindowUpdateFrame winup;
                winup.id = conn_id;
                winup.increment = default_frame_size - 9;
                apply_conn_frames(winup, server, client);
                winup.id = client.stream_1.id();
                apply_stream_frames<1>(winup, server, client, State::open, State::open);
                data.flag |= frame::Flag::end_stream;
                apply_stream_frames<1>(data, client, server, State::half_closed_local, State::half_closed_remote);
                data.data = {};
                apply_stream_frames<1>(data, server, client, State::closed, State::closed);
                return true;
            }

            static_assert(check_open());
        }  // namespace test

    }  // namespace fnet::http2::stream
}  // namespace futils
