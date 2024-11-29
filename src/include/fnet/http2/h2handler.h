/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "h2conn.h"
#include "h2stream.h"
#include <unordered_map>

namespace futils {
    namespace fnet::http2 {
        enum class HTTP2Role {
            client,
            server,
        };

        struct FrameHandler : std::enable_shared_from_this<FrameHandler> {
           private:
            stream::Conn conn;
            std::unordered_map<std::uint32_t, std::shared_ptr<stream::Stream>> streams;
            flex_storage recv_buffer;
            bool preface = false;

           public:
            FrameHandler(HTTP2Role role)
                : conn(role == HTTP2Role::server) {}

            void send_preface() {
                if (!conn.state.is_server() && !preface) {  // is client
                    conn.send_buffer.stream([&](binary::writer& w) {
                        w.write(http2_connection_preface);
                    });
                }
            }

            constexpr bool is_closed() const {
                return conn.state.is_closed();
            }

            futils::binary::WriteStreamingBuffer<flex_storage>& get_write_buffer() {
                return conn.send_buffer;
            }

            std::shared_ptr<stream::Stream> find_stream(stream::ID id) {
                auto found = streams.find(id);
                if (found == streams.end()) {
                    return nullptr;
                }
                return found->second;
            }

            std::shared_ptr<stream::Stream> open() {
                auto id = conn.state.new_stream();
                if (!id.valid()) {
                    return nullptr;
                }
                auto self = shared_from_this();
                auto conn_ptr = std::shared_ptr<stream::Conn>(self, &conn);
                auto s = std::allocate_shared<stream::Stream>(glheap_allocator<stream::Stream>{}, id, std::move(conn_ptr), conn.state.local_settings().max_header_table_size);
                streams.emplace(id, s);
                return s;
            }

            Error recv_frame(const frame::Frame& f, auto&& stream_callback) {
                if (frame::is_connection_level(f.type) ||
                    (f.type == frame::Type::window_update &&
                     f.id == 0)) {
                    if (f.id != 0) {
                        return Error{H2Error::protocol, false, "unexpected frame id"};
                    }
                    return conn.recv_frame(f, [&](std::uint32_t old_initial, std::uint32_t new_initial) {
                        for (auto& [_, s] : streams) {
                            s->state.set_new_peer_window(old_initial, new_initial);
                        }
                    });
                }
                auto found = streams.find(f.id);
                std::shared_ptr<stream::Stream> s;
                if (found == streams.end()) {
                    auto self = shared_from_this();
                    auto conn_ptr = std::shared_ptr<stream::Conn>(self, &conn);
                    s = std::allocate_shared<stream::Stream>(glheap_allocator<stream::Stream>{}, f.id, std::move(conn_ptr), conn.state.local_settings().max_header_table_size);
                    streams.emplace(f.id, s);
                }
                else {
                    s = found->second;
                }
                if (auto err = s->recv_frame(f)) {
                    return err;
                }
                stream_callback(s);
                return no_error;
            }

            Error read_frames(auto&& stream_callback) {
                bool ok = false;
                while (true) {
                    binary::reader r{recv_buffer};
                    if (conn.state.is_server() && !preface) {
                        constexpr auto preface_str = view::rcvec(http2_connection_preface);
                        if (r.remain().size() < preface_str.size()) {
                            break;
                        }
                        auto cmp = r.remain().substr(0, preface_str.size());
                        if (cmp != view::rvec(preface_str.begin(), preface_str.end())) {
                            return Error{H2Error::protocol, false, "invalid preface"};
                        }
                        preface = true;
                        recv_buffer.shift_front(preface_str.size());
                        continue;
                    }
                    auto err = frame::parse_frame(r, conn.state.local_settings().max_frame_size, [&](const auto& f, const frame::UnknownFrame&, Error err) {
                        ok = true;
                        if constexpr (std::is_same_v<decltype(f), const frame::UnknownFrame&>) {
                            return no_error;  // ignore
                        }
                        else {
                            if (err) {
                                return err;
                            }
                            return recv_frame(f, stream_callback);
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

            void add_data(view::rvec data) {
                recv_buffer.append(data);
            }

            Error add_data_and_read_frames(view::rvec data, auto&& stream_callback) {
                add_data(data);
                return read_frames(stream_callback);
            }

            Error send_settings(auto&& set_settings) {
                return conn.send_settings(set_settings, [&](std::uint32_t old_window, std::uint32_t new_window) {
                    for (auto& [_, s] : streams) {
                        s->state.set_new_local_window(old_window, new_window);
                    }
                });
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

#define WRITE_IF(key, value)                                       \
    if ((settings.value) != (conn.state.local_settings().value)) { \
        write(key, settings.value);                                \
    }
                    if (diff) {
                        WRITE_IF(k(sk::max_header_table_size), max_header_table_size);
                        WRITE_IF(k(sk::enable_push), enable_push ? 1 : 0);
                        WRITE_IF(k(sk::max_concurrent), max_concurrent_stream);
                        WRITE_IF(k(sk::initial_windows_size), initial_window_size);
                        WRITE_IF(k(sk::max_frame_size), max_frame_size);
                        WRITE_IF(k(sk::header_list_size), max_header_list_size);
#undef WRITE_IF
                    }
                    else {
                        write(k(sk::max_header_table_size), settings.max_header_table_size);
                        write(k(sk::enable_push), settings.enable_push ? 1 : 0);
                        write(k(sk::max_concurrent), settings.max_concurrent_stream);
                        write(k(sk::initial_windows_size), settings.initial_window_size);
                        write(k(sk::max_frame_size), settings.max_frame_size);
                        write(k(sk::header_list_size), settings.max_header_list_size);
                    }
                });
            }
        };

        constexpr auto sizeof_FrameHandler = sizeof(FrameHandler);
        constexpr auto sizeof_Stream = sizeof(stream::Stream);
    }  // namespace fnet::http2
}  // namespace futils
