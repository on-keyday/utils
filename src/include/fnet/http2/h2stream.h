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
    namespace fnet::http2 {
        struct FrameHandler;
        namespace stream {

            struct Stream {
               private:
                std::weak_ptr<Conn> conn;
                std::shared_ptr<void> application_data;
                flex_storage data_buffer;
                flex_storage header_buffer;
                slib::deque<std::pair<flex_storage, flex_storage>> promised_header;
                slib::deque<std::pair<flex_storage, flex_storage>> received_header;
                stream::StreamState state;
                std::uint32_t recv_table_size_limit = 0;

                friend struct http2::FrameHandler;

                Error recv_frame(const frame::Frame& f) {
                    auto c = conn.lock();
                    if (!c) {
                        return Error{H2Error::transport, false, "connection already closed"};
                    }
                    auto continuous = state.recv_continuous_state();
                    if (auto err = c->recv_stream_frame(f, state)) {
                        return err;
                    }
                    if (f.type == frame::Type::data) {
                        data_buffer.append(static_cast<const frame::DataFrame&>(f).data);
                        return no_error;
                    }
                    auto handle_header = [&](view::rvec payload, auto& header) {
                        auto header_cb = [&](auto&& key, auto&& value) {
                            if constexpr (std::is_same_v<std::decay_t<decltype(key)>, const char*>) {
                                header.push_back(std::pair{flex_storage{key}, flex_storage{value}});
                            }
                            else {
                                header.push_back(std::pair{std::forward<decltype(key)>(key), std::forward<decltype(value)>(value)});
                            }
                        };
                        auto err = hpack::decode<flex_storage>(payload, c->decode_table, header_cb, recv_table_size_limit, c->state.local_settings().max_header_table_size);
                        if (err != hpack::HpackError::none) {
                            return Error{H2Error::compression};
                        }
                        return no_error;
                    };
                    if (f.type == frame::Type::header) {
                        if (f.flag & frame::Flag::end_headers) {
                            return handle_header(static_cast<const frame::HeaderFrame&>(f).data, received_header);
                        }
                        header_buffer.append(static_cast<const frame::HeaderFrame&>(f).data);
                    }
                    if (f.type == frame::Type::push_promise) {
                        if (f.flag & frame::Flag::end_headers) {
                            return handle_header(static_cast<const frame::PushPromiseFrame&>(f).data, promised_header);
                        }
                        header_buffer.append(static_cast<const frame::PushPromiseFrame&>(f).data);
                    }
                    if (f.type == frame::Type::continuation) {
                        auto& cont = static_cast<const frame::ContinuationFrame&>(f);
                        header_buffer.append(cont.data);
                        if (!(f.flag & frame::Flag::end_headers)) {
                            return no_error;  // wait for next CONTINUATION
                        }
                        if (auto err = handle_header(header_buffer, continuous == ContinuousState::on_header ? received_header : promised_header)) {
                            return err;
                        }
                        header_buffer.clear();
                    }
                    return no_error;
                }

               public:
                Stream(ID id, std::weak_ptr<Conn> conn, std::uint32_t table_size_limit)
                    : state(id), conn(conn), recv_table_size_limit(table_size_limit) {}

                State stream_state() const {
                    return state.state();
                }

                http1::HTTPState recv_state() const {
                    return state.recv_state();
                }

                template <class T>
                std::shared_ptr<T> get_or_set_application_data(auto&& create) {
                    auto setter = [&](auto&& to_set) {
                        application_data = std::forward<decltype(to_set)>(to_set);
                    };
                    return create(std::static_pointer_cast<T>(application_data), setter);
                }

                http1::header::HeaderErr receive_header(auto&& cb) {
                    if (state.recv_state() != http1::HTTPState::init &&
                        state.recv_state() != http1::HTTPState::header) {
                        return http1::header::HeaderError::invalid_state;
                    }
                    state.set_recv_state(http1::HTTPState::header);
                    if (received_header.size() == 0) {
                        return http1::header::HeaderError::no_data;
                    }
                    for (auto& [key, value] : received_header) {
                        cb(key, value);
                    }
                    if (state.state() == State::half_closed_remote) {
                        state.set_recv_state(http1::HTTPState::end);
                    }
                    else {
                        state.set_recv_state(http1::HTTPState::body);
                    }
                    received_header.clear();
                    return http1::header::HeaderError::none;
                }

                http1::body::BodyReadResult receive_data(auto&& cb) {
                    if (state.recv_state() != http1::HTTPState::body) {
                        return http1::body::BodyReadResult::invalid_state;
                    }
                    if (data_buffer.size() == 0) {
                        if (state.state() == State::closed) {
                            state.set_recv_state(http1::HTTPState::end);
                            return http1::body::BodyReadResult::full;
                        }
                        return http1::body::BodyReadResult::incomplete;
                    }
                    cb(data_buffer);
                    if (state.state() == State::closed) {
                        state.set_recv_state(http1::HTTPState::end);
                    }
                    data_buffer.clear();
                    return http1::body::BodyReadResult::full;
                }

                Error send_data(view::rvec data, bool fin, byte* padding = nullptr) {
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

                Error send_headers(auto&& header, bool fin, bool add_header_table = false, frame::Priority* prio = nullptr, byte* padding = nullptr) {
                    auto c = conn.lock();
                    if (!c) {
                        return Error{H2Error::transport};  // connection already closed
                    }
                    return c->send_with_hpack(state, header, add_header_table, [&](auto&& send, auto&& get_data) {
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
                        return Error{H2Error::protocol, true, "cannot send PUSH_PROMISE by client"};
                    }
                    return c->send_with_hpack(state, request_header, add, [&](auto&& send, auto&& get_data) {
                        frame::PushPromiseFrame head{};
                        head.id = state.id();
                        head.promise = reserve_id;
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
        }  // namespace stream
    }  // namespace fnet::http2
}  // namespace futils