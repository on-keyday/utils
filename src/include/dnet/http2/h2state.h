/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// h2state - http2 state machines
#pragma once
#include "h2frame.h"
#include <deprecated/net/http2/stream_state.h>
#include "h2frame.h"
#include "h2err.h"
#include "h2settings.h"
#include "../std/deque.h"
#include "../dll/allocator.h"
#include "../storage.h"

namespace utils {
    namespace dnet::http2::stream {

        using State = utils::net::http2::Status;

        struct StreamNumCommonState {
            // stream id
            int id = 0;
            // stream state machine
            State state = State::idle;
            // error codes
            ErrCode errs;
        };

        struct StreamNumDirState {
            // flow control window
            std::int64_t window = 65535;
            // previous processed frame type
            frame::FrameType preproced_type = frame::FrameType::settings;
            // previous processed is
            // - FrameType::header without Flag.end_headers
            // - FrameType::contiuous without Flag.end_headers
            bool on_continuous = false;
        };

        struct StreamNumState {
            // common stream state
            StreamNumCommonState com;
            // recv window(recv data,send window_update)
            StreamNumDirState recv;
            // send window(send data,recv window_update)
            StreamNumDirState send;
        };

        // change_state_send changes http2 protocol state following sender role
        // state is represented at Section 5.1 of RFC7540
        // this function would changes s.err and on_cotinuous parameter
        // if state change succeeded returns true
        // otherwise returns false and library error code is set to s.err
        constexpr bool change_state_send(State& state, ErrCode& errs, bool& on_continuous, const frame::Frame& f) {
            using t = frame::FrameType;
            using l = frame::Flag;
            using a = State;
            if (on_continuous) {
                if (f.type != t::continuous) {
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
            using t = frame::FrameType;
            using l = frame::Flag;
            using a = State;
            if (on_continuous) {
                if (f.type != t::continuous) {
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
        }

        struct ConnNumDirState {
            // previous read_frame/send_frame called id
            std::int32_t preproced_id = 0;
            // previous read_frame/send_frame called type
            frame::FrameType preproced_type = frame::FrameType::invalid;
            // previous read_frame/send_frame called id excluding id 0
            std::int32_t preproced_stream_id = 0;
            // previous read_frame/send_frame called type excluding id 0
            frame::FrameType preproced_stream_type = frame::FrameType::invalid;
            // predefine settings
            h2set::PredefinedSettings settings;
        };

        struct ConnNumState {
            // stream 0 states (window)
            StreamNumState s0;
            // max sent/recieved frame id
            std::uint32_t max_issued_id = 0;
            // max Status == closed frame id
            std::uint32_t max_closed_id = 0;
            // recv preproced_id/remote settings
            ConnNumDirState recv;
            // send preproced_id/local settings
            ConnNumDirState send;
            // is server mode
            bool is_server;
        };

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

        constexpr bool is_stream_level(frame::FrameType type) {
            using t = frame::FrameType;
            return type == t::data || type == t::header ||
                   type == t::priority || type == t::rst_stream ||
                   type == t::push_promise || type == t::continuous;
        }

        constexpr bool is_connection_level(frame::FrameType type) {
            using t = frame::FrameType;
            return type == t::settings || type == t::ping || type == t::goaway;
        }

        // has_frame_state_error  detects error state between stream state and frame type.
        // this function would not detect error that parse_frame/render_frame function or change_state_* function detects.
        // this behaviour is to follow DRY principle.
        constexpr bool has_frame_state_error(
            bool send, h2set::PredefinedSettings& ps, State state, ErrCode& errs, frame::Frame& f) {
            using t = frame::FrameType;
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
            StreamNumDirState *dir = nullptr, *s0dir = nullptr,
                              *rdir = nullptr, *r0dir = nullptr;
            if (on_send) {
                dir = &s.send;
                s0dir = &c.s0.send;  // sending data -> effects to send parameter
                rdir = &s.recv;
                r0dir = &c.s0.recv;  // sending window_update -> effects to recv parameter
            }
            else {
                dir = &s.recv;
                s0dir = &c.s0.recv;  // receiving data  -> effects to recv parameter
                rdir = &s.send;
                r0dir = &c.s0.send;  // receiving winodw_update -> effects to send parameter
            }
            if (f.type == frame::FrameType::data) {
                if (s.com.id != f.id) {
                    s.com.errs.err = http2_invalid_id;
                    s.com.errs.debug = "s.com.id != f.id on DataFrame";
                    return false;
                }
                dir->window -= f.len;
                s0dir->window -= f.len;
            }
            else if (f.type == frame::FrameType::window_update) {
                auto& w = static_cast<frame::WindowUpdateFrame&>(f);
                if (w.id == 0) {
                    r0dir->window += w.increment;
                }
                else if (w.id == s.com.id) {
                    rdir->window += w.increment;
                }
                else {
                    s.com.errs.err = http2_invalid_id;
                    s.com.errs.debug = "f.id != s.com.id&& f.id != 0 on WindowUpdateFrame";
                    return false;
                }
            }
            return true;
        }

        constexpr void set_new_window(std::int64_t& window, std::uint32_t old_window, std::uint32_t new_window) {
            window = new_window - (old_window - window);
        }

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
                if (c.max_issued_id > s.com.id) {
                    s.com.state = State::closed;  // implicit closed
                }
            }
        }

        constexpr size_t get_sendable_size(StreamNumDirState& c, StreamNumDirState& s) {
            if (c.window < s.window) {
                return c.window;
            }
            else {
                return s.window;
            }
        }

        constexpr bool check_data_window(std::int64_t cwindow, std::int64_t swindow, ErrCode& errs, frame::Frame& f) {
            if (f.type == frame::FrameType::data) {
                if (cwindow - f.len < 0 || swindow - f.len < 0) {
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
        }

    }  // namespace dnet::http2::stream
}  // namespace utils
