/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "h2state.h"

namespace utils {
    namespace fnet::http2 {
        namespace stream {
            struct Conn {
                stream::ConnNumState state;
                io::expand_writer<flex_storage> send_buffer;

                Error recv_frame(const frame::Frame& f) {
                    if (auto err = state.on_recv_frame(f)) {
                        return err;
                    }
                    if (f.type == frame::Type::ping) {
                        if (f.flag & frame::Flag::ack) {
                            return no_error;
                        }
                        return send_ping(static_cast<const frame::PingFrame&>(f).opaque, true);
                    }
                    return no_error;
                }

                Error send_ping(std::uint64_t opaque, bool ack = false) {
                    frame::PingFrame p;
                    p.id = conn_id;
                    p.opaque = opaque;
                    if (ack) {
                        p.flag |= frame::Flag::ack;
                    }
                    if (auto err = state.on_send_frame(p)) {
                        return err;
                    }
                    if (!frame::render_frame(send_buffer, p)) {
                        return Error{H2Error::internal, false, "failed to render PING frame"};
                    }
                    return no_error;
                }

                Error send_settings(auto&& set_settings) {
                    frame::SettingsFrame f;
                    io::expand_writer<flex_storage> s;
                    auto key_value = [&](std::uint16_t key, std::uint32_t value) {
                        return io::write_num(s, key) && io::write_num(s, value);
                    };
                    set_settings(key_value);
                    f.settings = s.written();
                    if (auto err = state.on_send_frame(f)) {
                        return err;
                    }
                    if (!frame::render_frame(send_buffer, f)) {
                        return Error{H2Error::none, false, "failed to render SETTINGS frame"};
                    }
                    return no_error;
                }

                Error send_settings_ack() {
                    frame::SettingsFrame f;
                    f.flag |= frame::Flag::ack;
                    if (auto err = state.on_send_frame(f)) {
                        return err;
                    }
                    if (!frame::render_frame(send_buffer, f)) {
                        return Error{H2Error::none, false, "failed to render SETTINGS frame"};
                    }
                    return no_error;
                }

                Error send_goaway(std::uint32_t code, view::rvec debug) {
                    frame::GoAwayFrame f;
                    f.id = conn_id;
                    f.code = code;
                    f.debug = debug;
                    f.processed_id = state.max_processed;
                    if (auto err = state.on_send_frame(f)) {
                        return err;
                    }
                    if (!frame::render_frame(send_buffer, f)) {
                        return Error{H2Error::internal, false, "failed to render GOAWAY frame"};
                    }
                    return no_error;
                }
            };
        }  // namespace stream
    }      // namespace fnet::http2
}  // namespace utils
