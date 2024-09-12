/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "h2state.h"
#include "h2conn.h"
#include "../util/hpack/hpack.h"

namespace futils {
    namespace fnet::http2::stream {

        struct Stream {
            std::weak_ptr<Conn> conn;
            stream::StreamState state;
            flex_storage data_buffer;
            slib::deque<std::pair<flex_storage, flex_storage>> promised_header;
            slib::deque<std::pair<flex_storage, flex_storage>> received_header;
            std::uint32_t table_size_limit = 0;

            Error recv_frame(const frame::Frame& f) {
                auto c = conn.lock();
                if (!c) {
                    return Error{H2Error::transport};  // connection already closed
                }
                if (auto err = c->recv_stream_frame(f, state)) {
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
                    auto err = hpack::decode<flex_storage>(payload, c->decode_table, header, table_size_limit, c->state.recv_settings().max_header_table_size);
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

            Error send_data(view::rvec data, bool fin, byte* padding) {
                auto c = conn.lock();
                if (!c) {
                    return Error{H2Error::transport};  // connection already closed
                }
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
                return c->send_stream_frame(f, state, "failed to render DATA frame");
            }

            Error send_headers(auto&& header, bool add, bool fin, frame::Priority* prio, byte* padding) {
                auto c = conn.lock();
                if (!c) {
                    return Error{H2Error::transport};  // connection already closed
                }
                return c->send_with_hpack(state, header, add, [&](auto&& send, auto&& get_data) {
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
                    // get_data returns whether data is final
                    if (get_data(head.data)) {
                        head.flag |= frame::Flag::end_headers;
                    }
                    return send(head, "failed to render HEADER frame");
                });
            }

            Error send_rst_stream(std::uint32_t code) {
                auto c = conn.lock();
                if (!c) {
                    return Error{H2Error::transport};  // connection already closed
                }
                frame::RstStreamFrame rst;
                rst.id = state.id();
                rst.code = code;
                return c->send_stream_frame(rst, state, "failed to render RST_STREAM frame");
            }

            Error send_window_update(std::int32_t increment) {
                if (increment <= 0) {
                    return Error{H2Error::protocol, true, "need more than 0"};
                }
                auto c = conn.lock();
                if (!c) {
                    return Error{H2Error::transport};  // connection already closed
                }
                frame::WindowUpdateFrame window;
                window.id = state.id();
                window.increment = increment;
                return c->send_stream_frame(window, state, "failed to render WINDOW_UPDATE");
            }

            Error send_push_promise(ID reserve_id, auto&& request_header, bool add, byte* padding) {
                auto c = conn.lock();
                if (!c) {
                    return Error{H2Error::transport};  // connection already closed
                }
                if (!reserve_id.by_server() ||
                    !state.id().by_server()) {
                    return Error{H2Error::protocol, "cannot send PUSH_PROMISE by client"};
                }
                return c->send_with_hpack(state, request_header, add, [&](auto&& send, auto&& get_data) {
                    frame::PushPromiseFrame head{};
                    head.id = state.id();
                    head.promise = reserve.id();
                    if (padding) {
                        head.flag |= frame::Flag::padded;
                        head.padding = *padding;
                    }
                    // get_data returns whether data is final
                    if (get_data(head.data)) {
                        head.flag |= frame::Flag::end_headers;
                    }
                    return send(head, "failed to render PUSH_PROMISE frame");
                });
            }
        };
    }  // namespace fnet::http2::stream
}  // namespace futils
