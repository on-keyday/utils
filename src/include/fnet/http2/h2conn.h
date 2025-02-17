/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "h2state.h"
#include "../util/hpack/hpack.h"

namespace futils {
    namespace fnet::http2 {
        struct FrameHandler;
        namespace stream {
            struct Stream;

            struct Conn {
               private:
                stream::ConnState state;
                binary::WriteStreamingBuffer<flex_storage> send_buffer;

                slib::deque<std::pair<flex_storage, flex_storage>> encode_table;
                slib::deque<std::pair<flex_storage, flex_storage>> decode_table;
                friend struct Stream;
                friend struct ::futils::fnet::http2::FrameHandler;

                Error write_frame(const frame::Frame& f, const char* err_msg) {
                    if (!send_buffer.stream([&](auto& w) { return render_frame(w, f); })) {
                        return Error{H2Error::internal, false, err_msg};
                    }
                    return no_error;
                }

                Error recv_stream_frame(const frame::Frame& f, StreamState& s) {
                    return state.on_recv_stream_frame(f, s);
                }

                Error send_stream_frame(const frame::Frame& f, StreamState& s, const char* err_msg) {
                    if (auto err = state.can_send_stream_frame(f, s)) {
                        return err;
                    }
                    if (auto err = state.on_send_stream_frame(f, s)) {
                        return err;
                    }
                    return write_frame(f, err_msg);
                }

                Error send_with_hpack(StreamState& s, auto&& header, bool add, auto&& cb) {
                    auto settings = state.peer_settings();
                    auto frame_size = settings.max_frame_size;
                    flex_storage payload_buf;
                    slib::deque<std::pair<flex_storage, flex_storage>> del_entry;
                    size_t insert_count = 0;
                    auto base_size = encode_table.size();
                    auto on_modify_table = [&](auto&& key, auto&& value, bool insert) {
                        if (del_entry.size() == base_size) {
                            return;
                        }
                        if (insert) {
                            insert_count++;
                        }
                        else {
                            del_entry.push_front(std::make_pair(
                                hpack::internal::convert_string<flex_storage>(key),
                                hpack::internal::convert_string<flex_storage>(value)));
                        }
                    };
                    hpack::HpackError res = hpack::encode(payload_buf, encode_table, header, settings.max_header_table_size, add, on_modify_table);
                    if (res != hpack::HpackError::none) {
                        return Error{H2Error::compression, false, "hpack error", res};
                    }
                    auto recover_table = helper::defer([&] {
                        if (del_entry.size() == base_size) {
                            encode_table = std::move(del_entry);
                        }
                        else {
                            while (insert_count) {
                                encode_table.pop_back();
                                insert_count--;
                            }
                            for (auto& ent : del_entry) {
                                encode_table.push_back(std::move(ent));
                            }
                        }
                    });
                    binary::reader progress{payload_buf};
                    auto increment_progress = [&](view::rvec& data) {
                        data = progress.read_best(frame_size);
                        return progress.empty();
                    };
                    auto call = [&](const frame::Frame& f, const char* err_msg) {
                        if (auto err = state.can_send_stream_frame(f, s)) {
                            return err;
                        }
                        recover_table.cancel();
                        if (auto err = state.on_send_stream_frame(f, s)) {
                            return err;
                        }
                        return send_buffer.stream([&](binary::writer& w) {
                            if (!frame::render_frame(w, f)) {
                                return Error{H2Error::internal, false, err_msg};
                            }
                            return no_error;
                        });
                    };
                    if (auto err = cb(call, increment_progress)) {
                        return err;
                    }
                    while (!progress.empty()) {
                        frame::ContinuationFrame fr{};
                        fr.id = s.id();
                        view::rvec data;
                        increment_progress(data);
                        fr.data = data;
                        if (progress.empty()) {
                            fr.flag |= frame::Flag::end_headers;
                        }
                        if (auto err = state.on_send_stream_frame(fr, s)) {
                            return err;
                        }
                        auto err = send_buffer.stream([&](binary::writer& w) {
                            if (!frame::render_frame(w, fr)) {
                                return Error{H2Error::internal, false, "failed to render CONTINUATION frame"};
                            }
                            return no_error;
                        });
                        if (err) {
                            return err;
                        }
                    }
                    return no_error;
                }

               public:
                Conn(bool is_server)
                    : state(is_server) {}
                Error recv_frame(const frame::Frame& f, auto&& on_initial_window_update) {
                    if (auto err = state.on_recv_frame(f, on_initial_window_update)) {
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
                    if (auto err = state.on_send_frame(p, [](auto&&...) {})) {
                        return err;
                    }
                    return write_frame(p, "failed to render PING frame");
                }

                Error send_settings(auto&& set_settings, auto&& on_initial_window_update) {
                    frame::SettingsFrame f;
                    binary::WriteStreamingBuffer<flex_storage> tmp;
                    tmp.stream([&](auto& s) {
                        auto key_value = [&](std::uint16_t key, std::uint32_t value) {
                            return binary::write_num(s, key) && binary::write_num(s, value);
                        };
                        set_settings(key_value);
                        f.settings = s.written();
                    });
                    if (auto err = state.on_send_frame(f, on_initial_window_update)) {
                        return err;
                    }
                    return write_frame(f, "failed to render SETTINGS frame");
                }

                Error send_settings_ack() {
                    frame::SettingsFrame f;
                    f.flag |= frame::Flag::ack;
                    if (auto err = state.on_send_frame(f, [](auto&&...) {})) {
                        return err;
                    }
                    return write_frame(f, "failed to render SETTINGS+ACK frame");
                }

                Error send_goaway(std::uint32_t code, view::rvec debug) {
                    frame::GoAwayFrame f;
                    f.id = conn_id;
                    f.code = code;
                    f.debug = debug;
                    f.processed_id = state.get_max_processed();
                    if (auto err = state.on_send_frame(f, [](auto&&...) {})) {
                        return err;
                    }
                    return write_frame(f, "failed to render GOAWAY frame");
                }
            };
        }  // namespace stream
    }  // namespace fnet::http2
}  // namespace futils
