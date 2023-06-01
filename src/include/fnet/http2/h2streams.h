/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "h2state.h"
#include "../../net_util/hpack/hpack.h"

namespace utils {
    namespace fnet::http2::stream {

        struct Stream {
            stream::StreamNumState state;
            slib::deque<std::pair<flex_storage, flex_storage>> encode_table;
            slib::deque<std::pair<flex_storage, flex_storage>> decode_table;
            flex_storage data_buffer;
            slib::deque<std::pair<flex_storage, flex_storage>> promised_header;
            slib::deque<std::pair<flex_storage, flex_storage>> received_header;
            std::uint32_t table_size_limit = 0;

            Error recv_frame(ConnNumState& conn, const frame::Frame& f) {
                if (auto err = conn.on_recv_stream_frame(f, state)) {
                    return err;
                }
                if (f.type == frame::Type::data) {
                    data_buffer.append(static_cast<const frame::DataFrame&>(f).data);
                    return no_error;
                }
                auto handle_header = [&](view::rvec payload, auto& header) {
                    auto header = [&](auto&& key, auto&& value) {
                        header.push_back({std::forward<decltype(key)>(key), std::forward<decltype(value)>(value)});
                    };
                    auto err = hpack::decode<flex_storage>(payload, decode_table, header, table_size_limit, conn.recv.settings.header_table_size);
                    if (err != hpack::HpackError::none) {
                        return Error{H2Error::compression};
                    }
                    return no_error;
                };
                if (f.type == frame::Type::header) {
                    return handle_header(static_cast<const frame::HeaderFrame&>(f).data, received_header);
                }
                if (f.type == frame::Type::push_promise) {
                    return handle_header(static_cast<const frame::PushPromiseFrame&>(f).data, promised_header);
                }
                return no_error;
            }

            template <class T>
            Error send_data(io::expand_writer<T>& w, ConnNumState& conn, view::rvec data, bool fin, byte* padding) {
                auto frame_size = conn.send_settings().max_frame_size;
                frame::DataFrame f;
                f.id = state.id();
                f.data = data;
                if (padding) {
                    f.flag |= frame::Flag::padded;
                    f.padding = *padding;
                }
                if (fin) {
                    f.flag |= frame::Flag::end_stream;
                }
                if (auto err = conn.can_send_stream_frame(f, state)) {
                    return err;
                }
                if (auto err = conn.on_send_stream_frame(f, state)) {
                    return err;
                }
                if (!frame::render_frame(w, f)) {
                    return Error{H2Error::internal, false, "failed to render DATA frame"};
                }
                return no_error;
            }

            template <class T>
            Error with_hpack_recover(io::expand_writer<T>& w, ConnNumState& conn, auto&& header, bool add, auto&& cb) {
                auto settings = conn.send_settings();
                auto frame_size = settings.max_frame_size;
                flex_storage payload_buf;
                slib::deque<std::pair<flex_storage, flex_storage>> del_entry;
                size_t insert_count = 0;
                auto base_size = encode_table.size();
                auto on_modify_table = [](auto&& key, auto&& value, bool insert) {
                    if (del_entry.size() == base_size) {
                        return;
                    }
                    if (insert) {
                        insert_count++;
                    }
                    else {
                        del_entry.push_front({key, value});
                    }
                };
                auto err = hpack::encode(payload_buf, encode_table, header, settings.header_table_size, add, on_modify_table);
                if (err) {
                    return Error{H2Error::compression};
                }
                auto recover_table = helper::defer([&] {
                    if (del_entry == base_size) {
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
                io::reader progress{payload_buf};
                auto increment_progress = [&](view::rvec& data) {
                    data = progress.read_best(frame_size);
                };
                if (auto err = cb(recover_table, progress, increment_progress)) {
                    return err;
                }
                while (!progress.empty()) {
                    frame::ContinuationFrame fr{};
                    fr.id = state.id();
                    increment_progress();
                    if (progress.empty()) {
                        fr.flag |= frame::Flag::end_headers;
                    }
                    if (auto err = conn.on_send_stream_frame(fr, state)) {
                        return err;
                    }
                    if (!frame::render_frame(w, fr)) {
                        return Error{H2Error::internal, false, "failed to render CONTINUATION frame"};
                    }
                }
            }

            template <class T>
            Error send_headers(io::expand_writer<T>& w, ConnNumState& conn, auto&& header, bool add, bool fin, frame::Priority* prio, byte* padding) {
                return with_hpack_recover(w, conn, header, add, [&](auto&& recover, io::reader& progress, auto&& increment_progress) {
                    frame::HeaderFrame head{};
                    head.id = state.id();
                    if (prio) {
                        head.flag |= frame::Flag::priority;
                        head.priority = *prio;
                    }
                    if (padding) {
                        head.flag |= frame::Flag::padded;
                        head.padding = *padding;
                    }
                    if (fin) {
                        head.flag |= frame::Flag::end_stream;
                    }
                    increment_progress(head.data);
                    if (progress.empty()) {
                        head.flag |= frame::Flag::end_headers;
                    }
                    if (auto err = conn.can_send_stream_frame(head, state)) {
                        return err;
                    }
                    recover.cancel();  // no way to recover after here
                    if (auto err = conn.on_send_stream_frame(head, state)) {
                        return err;
                    }
                    if (!frame::render_frame(w, head)) {
                        return Error{H2Error::internal, false, "failed to render HEADER frame"};
                    }
                    return no_error;
                });
            }

            template <class T>
            Error send_rst_stream(io::expand_writer<T>& w, ConnNumState& conn, std::uint32_t code) {
                auto frame_size = conn.send_settings().max_frame_size;
                frame::RstStreamFrame rst;
                rst.id = state.id();
                rst.code = code;
                if (auto err = conn.can_send_stream_frame(rst, state)) {
                    return err;
                }
                if (auto err = conn.on_send_stream_frame(rst, state)) {
                    return err;
                }
                if (!frame::render_frame(w, rst)) {
                    return Error{H2Error::internal, false, "failed to render RST_STREAM frame"};
                }
                return no_error;
            }

            template <class T>
            Error send_window_update(io::expand_writer<T>& w, ConnNumState& conn, std::int32_t increment) {
                if (increment <= 0) {
                    return Error{H2Error::protocol, true, "need more than 0"};
                }
                frame::WindowUpdateFrame window;
                window.id = state.id();
                window.increment = increment;
                if (auto err = conn.can_send_stream_frame(window, state)) {
                    return err;
                }
                if (auto err = conn.on_send_stream_frame(window, state)) {
                    return err;
                }
                if (!frame::render_frame(w, window)) {
                    return Error{H2Error::internal, false, "failed to render WINDOW_UPDATE frame"};
                }
                return no_error;
            }

            template <class T>
            Error send_push_promise(io::expand_writer<T>& w, ConnNumState& conn, StreamNumState& reserve, auto&& header, bool add, byte* padding) {
                if (!conn.send_settings().enable_push) {
                    return Error{H2Error::protocol, true, "push disabled"};
                }
                if (reserve.id().by_server() != state.id().by_server()) {
                    return Error{H2Error::protocol, true, "invalid promise id stream id"};
                }
                conn.maybe_closed_by_higher_creation(reserve);
                if (reserve.state() != State::idle) {
                    return Error{H2Error::protocol, false, "state invalid"};
                }
                return with_hpack_recover(w, conn, header, add, [&](auto&& recover, io::reader& progress, auto&& increment_progress) {
                    frame::PushPromiseFrame head{};
                    head.id = state.id();
                    head.promise = reserve.id();
                    if (padding) {
                        head.flag |= frame::Flag::padded;
                        head.padding = *padding;
                    }
                    increment_progress(head.data);
                    if (progress.empty()) {
                        head.flag |= frame::Flag::end_headers;
                    }
                    if (auto err = conn.can_send_stream_frame(head, state)) {
                        return err;
                    }
                    recover.cancel();  // no way to recover after here
                    if (auto err = conn.on_send_stream_frame(head, state)) {
                        return err;
                    }
                    if (!frame::render_frame(w, head)) {
                        return Error{H2Error::internal, false, "failed to render HEADER frame"};
                    }
                    return no_error;
                });
            }
        };
    }  // namespace fnet::http2::stream
}  // namespace utils
