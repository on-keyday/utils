/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "h2conn.h"
#include "h2stream.h"
#include <unordered_map>

namespace utils {
    namespace fnet::http2 {
        struct FrameHandler : std::enable_shared_from_this<FrameHandler> {
            stream::Conn conn;
            std::unordered_map<std::uint32_t, std::shared_ptr<stream::Stream>> streams;
            flex_storage recv_buffer;

            Error recv_frame(const frame::Frame& f) {
                if (frame::is_connection_level(f.type) ||
                    (f.type == frame::Type::window_update &&
                     f.id == 0)) {
                    if (f.id != 0) {
                        return Error{H2Error::protocol, false, "unexpected frame id"};
                    }
                    return conn.recv_frame(f);
                }
                auto found = streams.find(f.id);
                std::shared_ptr<stream::Stream> s;
                if (found == streams.end()) {
                    s = std::allocate_shared<stream::Stream>(glheap_allocator<stream::Stream>{});
                    streams.emplace(f.id, s);
                }
                else {
                    s = found->second;
                }
                if (auto err = s->recv_frame(f)) {
                    return err;
                }
            }

            Error read_frames() {
                bool ok = false;
                while (true) {
                    binary::reader r{recv_buffer};
                    auto err = frame::parse_frame(r, conn.state.recv.settings.max_frame_size, [&](const auto& f, const frame::UnknownFrame&, Error err) {
                        ok = true;
                        if constexpr (std::is_same_v<decltype(f), const frame::UnknownFrame&>) {
                            return no_error;  // ignore
                        }
                        else {
                            if (err) {
                                return err;
                            }
                            return recv_frame(f);
                        }
                    });
                    if (err) {
                        return err;
                    }
                    if (!ok) {
                        break;
                    }
                    recv_buffer.shift_front(r.offset());
                    ok = false;
                }
                return no_error;
            }

            Error add_recv(view::rvec data) {
                recv_buffer.append(data);
                return read_frames();
            }

            Error send_settings(auto&& set_settings) {
                return conn.send_settings(set_settings);
            }

            Error send_goaway(std::uint32_t code, view::rvec debug = {}) {
                return conn.send_goaway(code, debug);
            }

            Error send_ping(std::uint64_t opaque) {
                return conn.send_ping(opaque);
            }

            Error send_settings(setting::PredefinedSettings settings, bool diff = false) {
                return send_settings([&](auto&& write) {
                    using sk = setting::SettingKey;
#define WRITE_IF(key, value)                             \
    if ((settings.value) != (com.recv.settings.value)) { \
        write(key, settings.value);                      \
    }
                    if (diff) {
                        WRITE_IF(k(sk::table_size), max_header_table_size);
                        WRITE_IF(k(sk::enable_push), enable_push ? 1 : 0);
                        WRITE_IF(k(sk::max_concurrent), max_concurrent_stream);
                        WRITE_IF(k(sk::initial_windows_size), initial_window_size);
                        WRITE_IF(k(sk::max_frame_size), max_frame_size);
                        WRITE_IF(k(sk::header_list_size), max_header_list_size);
#undef WRITE_IF
                    }
                    else {
                        write(k(sk::table_size), settings.max_header_table_size);
                        write(k(sk::enable_push), settings.enable_push ? 1 : 0);
                        write(k(sk::max_concurrent), settings.max_concurrent_stream);
                        write(k(sk::initial_windows_size), settings.initial_window_size);
                        write(k(sk::max_frame_size), settings.max_frame_size);
                        write(k(sk::header_list_size), settings.max_header_list_size);
                    }
                });
            }
        };
    }  // namespace fnet::http2
}  // namespace utils
