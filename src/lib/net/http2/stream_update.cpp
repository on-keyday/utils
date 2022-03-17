/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"

#include "../../../include/net_util/hpack.h"
#include "../http/header_impl.h"
#include "frame_reader.h"
#include "stream_impl.h"

namespace utils {
    namespace net {
        namespace http2 {

            UpdateResult decode_hpack(wrap::string& raw, http::internal::HeaderImpl& header, internal::ConnectionImpl& conn) {
                auto err = hpack::decode(raw, conn.recv.decode_table, header, conn.recv.setting[k(SettingKey::table_size)]);
                if (!err) {
                    return {
                        .err = H2Error::compression,
                        .detail = StreamError::hpack_failed,
                    };
                }
                return {};
            }

            template <class Impl>
            struct LastUpdateConnection {
                Impl& impl;
                const Frame& frame;
                LastUpdateConnection(Impl& i, const Frame& f)
                    : impl(i), frame(f) {}
                ~LastUpdateConnection() {
                    impl.preproced_id = frame.id;
                    impl.preproced_type = frame.type;
                }
            };

            int Stream::id() const {
                return impl->id;
            }
            std::uint8_t Stream::priority() const {
                return impl->priority.weight;
            }

            std::int64_t Stream::window_size() const {
                return impl->recv_window;
            }

            Status Stream::status() const {
                return impl->status;
            }

            UpdateResult Connection::update_recv(const Frame& frame) {
                auto stream = impl->get_stream(frame.id);
                if (frame.len > impl->recv.setting[k(SettingKey::max_frame_size)]) {
                    bool stream_level = true;
                    if (frame.type == FrameType::settings ||
                        frame.type == FrameType::header || frame.type == FrameType::push_promise ||
                        frame.type == FrameType::continuous) {
                        stream_level = false;
                    }
                    return UpdateResult{
                        .err = H2Error::protocol,
                        .detail = StreamError::over_max_frame_size,
                        .id = frame.id,
                        .frame = frame.type,
                        .stream_level = stream_level,
                    };
                }
                LastUpdateConnection _{impl->recv, frame};
                if (impl->recv.continuous_id && frame.type != FrameType::continuous) {
                    return UpdateResult{
                        .err = H2Error::protocol,
                        .detail = StreamError::continuation_not_followed,
                        .id = frame.id,
                        .frame = frame.type,
                    };
                }
                switch (frame.type) {
                    case FrameType::ping: {
                        assert(frame.id == 0);
                        ASSERT_STREAM();
                        auto& ping = static_cast<const PingFrame&>(frame);
                        if (ping.flag & Flag::ack) {
                            if (ping.opeque != impl->prev_ping) {
                                return {
                                    .err = H2Error::none,
                                    .detail = StreamError::ping_maybe_failed,
                                };
                            }
                        }
                        return {};
                    }
                    case FrameType::data: {
                        assert(frame.id != 0);
                        ASSERT_STREAM();
                        if (stream->status() != Status::open && stream->status() != Status::half_closed_local) {
                            return UpdateResult{
                                .err = H2Error::protocol,
                                .detail = StreamError::not_acceptable_on_current_status,
                                .id = frame.id,
                                .frame = FrameType::data,
                                .status = stream->status(),
                            };
                        }
                        auto& data = static_cast<const DataFrame&>(frame);
                        stream->impl->data.append(data.data);
                        stream->impl->recv_window -= data.data.size();
                        impl->recv.window -= data.data.size();
                        if (data.flag & Flag::end_stream) {
                            if (stream->status() == Status::half_closed_local) {
                                stream->impl->status = Status::closed;
                            }
                            else {
                                stream->impl->status = Status::half_closed_remote;
                            }
                        }
                        return {};
                    }
                    case FrameType::header: {
                        assert(frame.id != 0);
                        ASSERT_STREAM();
                        auto& header = static_cast<const HeaderFrame&>(frame);
                        if (stream->status() != Status::idle &&
                            stream->status() != Status::half_closed_local &&
                            stream->status() != Status::reserved_remote) {
                            return UpdateResult{
                                .err = H2Error::protocol,
                                .detail = StreamError::not_acceptable_on_current_status,
                                .id = frame.id,
                                .frame = FrameType::header,
                                .status = stream->status(),
                            };
                        }
                        stream->impl->remote_raw.append(header.data);
                        if (stream->status() == Status::idle) {
                            stream->impl->status = Status::open;
                        }
                        else {
                            stream->impl->status = Status::half_closed_local;
                        }
                        if (header.flag & Flag::priority) {
                            stream->impl->priority = header.priority;
                        }
                        if (header.flag & Flag::end_stream) {
                            if (stream->status() == Status::half_closed_local) {
                                stream->impl->status = Status::closed;
                            }
                            else {
                                stream->impl->status = Status::half_closed_remote;
                            }
                        }
                        if (header.flag & Flag::end_headers) {
                            return decode_hpack(stream->impl->remote_raw, stream->impl->h, *impl);
                        }
                        else {
                            impl->recv.continuous_id = header.id;
                        }
                        return {};
                    }
                    case FrameType::continuous: {
                        if (frame.id != impl->recv.preproced_id) {
                            return UpdateResult{
                                .err = H2Error::protocol,
                                .detail = StreamError::continuation_not_followed,
                                .id = 0,
                                .frame = FrameType::continuous,
                                .status = stream->status(),
                            };
                        }
                        ASSERT_STREAM();
                        auto& cont = static_cast<const Continuation&>(frame);
                        if (impl->recv.continuous_id == frame.id) {
                            stream->impl->remote_raw.append(cont.data);
                            if (cont.flag & Flag::end_headers) {
                                impl->recv.continuous_id = 0;
                                return decode_hpack(stream->impl->remote_raw, stream->impl->h, *impl);
                            }
                        }
                        else {
                            return UpdateResult{
                                .err = H2Error::internal,
                                .detail = StreamError::unimplemented,
                                .id = frame.id,
                                .frame = FrameType::continuous,
                            };  // unsafe implementation
                            auto to_add = impl->get_stream(impl->recv.continuous_id);
                            to_add->impl->local_raw.append(cont.data);
                            if (cont.flag & Flag::end_headers) {
                                impl->recv.continuous_id = 0;
                            }
                            return {};
                        }
                        return {};
                    }
                    case FrameType::window_update: {
                        auto& window_update = static_cast<const WindowUpdateFrame&>(frame);
                        if (window_update.id == 0) {
                            impl->send.window += window_update.increment;
                        }
                        else {
                            ASSERT_STREAM();
                            stream->impl->send_window += window_update.increment;
                        }
                        return {};
                    }
                    case FrameType::settings: {
                        assert(frame.id == 0);
                        assert(frame.len % 6 == 0);
                        if (frame.flag & Flag::ack) {
                            return {};
                        }
                        auto& settings = static_cast<const SettingsFrame&>(frame);
                        internal::FrameReader<const wrap::string&> r{settings.setting};
                        while (r.pos < r.ref.size()) {
                            std::uint16_t key = 0;
                            std::uint32_t value = 0;
                            if (!r.read(key)) {
                                return UpdateResult{
                                    .err = H2Error::internal,
                                    .detail = StreamError::internal_data_read,
                                    .id = 0,
                                    .frame = FrameType::settings,
                                };
                            }
                            if (!r.read(value)) {
                                return UpdateResult{
                                    .err = H2Error::internal,
                                    .detail = StreamError::internal_data_read,
                                    .id = 0,
                                    .frame = FrameType::settings,
                                };
                            }
                            auto& current = impl->recv.setting[key];
                            if (key == (std::uint16_t)SettingKey::initial_windows_size) {
                                impl->recv.window = value - current + impl->recv.window;
                                for (auto& stream : impl->streams) {
                                    auto& stwindow = stream.second.impl->recv_window;
                                    stwindow = value - current + stwindow;
                                }
                            }
                            current = value;
                        }
                        return {};
                    }
                    case FrameType::rst_stream: {
                        assert(frame.id != 0);
                        ASSERT_STREAM();
                        if (stream->status() == Status::idle) {
                            return UpdateResult{
                                .err = H2Error::protocol,
                                .detail = StreamError::not_acceptable_on_current_status,
                                .id = frame.id,
                                .frame = FrameType::rst_stream,
                                .status = Status::idle,
                            };
                        }
                        auto& rst = static_cast<const RstStreamFrame&>(frame);
                        stream->impl->code = rst.code;
                        stream->impl->status = Status::closed;
                        return {};
                    }
                    case FrameType::goaway: {
                        assert(frame.id == 0);
                        auto& goaway = static_cast<const GoAwayFrame&>(frame);
                        impl->code = goaway.code;
                        impl->debug_data = goaway.data;
                        return {};
                    }
                    case FrameType::priority: {
                        assert(frame.id != 0);
                        ASSERT_STREAM();
                        auto& prio = static_cast<const PriorityFrame&>(frame);
                        stream->impl->priority = prio.priority;
                        return {};
                    }
                    case FrameType::push_promise: {
                        if (!impl->recv.setting[k(SettingKey::enable_push)]) {
                            return UpdateResult{
                                .err = H2Error::protocol,
                                .detail = StreamError::push_promise_disabled,
                                .id = frame.id,
                                .frame = FrameType::push_promise,
                            };
                        }
                        assert(frame.id != 0);
                        ASSERT_STREAM();
                        if (stream->status() != Status::open && stream->status() != Status::half_closed_remote) {
                            return UpdateResult{
                                .err = H2Error::protocol,
                                .detail = StreamError::not_acceptable_on_current_status,
                                .id = frame.id,
                                .frame = FrameType::push_promise,
                                .status = stream->status(),
                            };
                        }
                        auto& promise = static_cast<const PushPromiseFrame&>(frame);
                        auto new_stream = impl->get_stream(promise.promise);
                        if (new_stream->status() != Status::idle) {
                            return UpdateResult{
                                .err = H2Error::protocol,
                            };
                        }
                        new_stream->impl->status = Status::reserved_remote;
                        if (!(frame.flag & Flag::end_headers)) {
                            impl->recv.continuous_id = promise.promise;
                        }
                        return {};
                    }
                    default: {
                        return UpdateResult{
                            .err = H2Error::none,
                            .detail = StreamError::unsupported_frame,
                            .frame = frame.type,
                        };
                    }
                }
            }

            UpdateResult Connection::update_send(const Frame& frame) {
                if (frame.len > impl->recv.setting[k(SettingKey::max_frame_size)]) {
                    bool stream_level = true;
                    if (frame.type == FrameType::settings ||
                        frame.type == FrameType::header || frame.type == FrameType::push_promise ||
                        frame.type == FrameType::continuous) {
                        stream_level = false;
                    }
                    return UpdateResult{
                        .err = H2Error::protocol,
                        .detail = StreamError::over_max_frame_size,
                        .id = frame.id,
                        .frame = frame.type,
                        .stream_level = stream_level,
                    };
                }
                LastUpdateConnection _{impl->send, frame};
                auto stream = impl->get_stream(frame.id);
                if (impl->send.continuous_id && frame.type != FrameType::continuous) {
                    return UpdateResult{
                        .err = H2Error::protocol,
                        .detail = StreamError::continuation_not_followed,
                        .id = frame.id,
                        .frame = frame.type,
                    };
                }
                switch (frame.type) {
                    case FrameType::data: {
                        ASSERT_STREAM();
                        if (stream->status() != Status::open && stream->status() != Status::half_closed_remote) {
                            return UpdateResult{
                                .err = H2Error::protocol,
                                .detail = StreamError::not_acceptable_on_current_status,
                                .id = frame.id,
                                .frame = FrameType::data,
                                .status = stream->status(),
                            };
                        }
                        stream->impl->send_window -= frame.len;
                        impl->send.window -= frame.len;
                        if (frame.flag & Flag::end_stream) {
                            if (stream->status() == Status::half_closed_remote) {
                                stream->impl->status = Status::closed;
                            }
                            else {
                                stream->impl->status = Status::half_closed_local;
                            }
                        }
                        return {};
                    }
                    case FrameType::header: {
                        ASSERT_STREAM();
                        if (stream->status() != Status::idle &&
                            stream->status() != Status::half_closed_remote &&
                            stream->status() != Status::reserved_local) {
                            return UpdateResult{
                                .err = H2Error::protocol,
                                .detail = StreamError::not_acceptable_on_current_status,
                                .id = frame.id,
                                .frame = FrameType::header,
                                .status = stream->status(),
                            };
                        }
                        auto& h = static_cast<const HeaderFrame&>(frame);
                        if (stream->status() == Status::idle) {
                            stream->impl->status = Status::open;
                        }
                        else {
                            stream->impl->status = Status::half_closed_remote;
                        }
                        if (frame.flag & Flag::priority) {
                            stream->impl->priority = h.priority;
                        }
                        if (frame.flag & Flag::end_stream) {
                            if (stream->status() == Status::half_closed_remote) {
                                stream->impl->status = Status::closed;
                            }
                            else {
                                stream->impl->status = Status::half_closed_local;
                            }
                        }
                        if (!(frame.flag & Flag::end_headers)) {
                            impl->send.continuous_id = frame.id;
                        }
                        return {};
                    }
                    case FrameType::continuous: {
                        if (frame.id != impl->send.continuous_id) {
                            return UpdateResult{
                                .err = H2Error::protocol,
                                .detail = StreamError::continuation_not_followed,
                                .id = 0,
                                .frame = FrameType::continuous,
                                .status = stream->status(),
                            };
                        }
                        if (frame.flag & Flag::end_headers) {
                            impl->send.continuous_id = 0;
                        }
                        return {};
                    }
                    case FrameType::window_update: {
                        auto& update = static_cast<const WindowUpdateFrame&>(frame);
                        if (frame.id == 0) {
                            impl->recv.window += update.increment;
                        }
                        else {
                            stream->impl->recv_window += update.increment;
                        }
                        return {};
                    }
                    case FrameType::rst_stream: {
                        if (stream->status() == Status::idle) {
                            return UpdateResult{
                                .err = H2Error::protocol,
                                .detail = StreamError::not_acceptable_on_current_status,
                                .id = frame.id,
                                .frame = FrameType::rst_stream,
                                .status = Status::idle,
                            };
                        }
                        auto& rst = static_cast<const RstStreamFrame&>(frame);
                        stream->impl->code = rst.code;
                        stream->impl->status = Status::closed;
                        return {};
                    }
                    case FrameType::goaway: {
                        auto& goaway = static_cast<const GoAwayFrame&>(frame);
                        impl->code = goaway.code;
                        impl->debug_data = goaway.data;
                        return {};
                    }
                    case FrameType::settings: {
                        assert(frame.id == 0);
                        assert(frame.len % 6 == 0);
                        if (frame.flag & Flag::ack) {
                            return {};
                        }
                        auto& settings = static_cast<const SettingsFrame&>(frame);
                        internal::FrameReader<const wrap::string&> r{settings.setting};
                        while (r.pos < r.ref.size()) {
                            std::uint16_t key = 0;
                            std::uint32_t value = 0;
                            if (!r.read(key)) {
                                return UpdateResult{
                                    .err = H2Error::internal,
                                    .detail = StreamError::internal_data_read,
                                    .id = 0,
                                    .frame = FrameType::settings,
                                };
                            }
                            if (!r.read(value)) {
                                return UpdateResult{
                                    .err = H2Error::internal,
                                    .detail = StreamError::internal_data_read,
                                    .id = 0,
                                    .frame = FrameType::settings,
                                };
                            }
                            auto& current = impl->send.setting[key];
                            if (key == k(SettingKey::initial_windows_size)) {
                                impl->send.window = value - current + impl->send.window;
                                for (auto& stream : impl->streams) {
                                    auto& stwindow = stream.second.impl->send_window;
                                    stwindow = value - current + stwindow;
                                }
                            }
                            current = value;
                        }
                        return {};
                    }
                    case FrameType::priority: {
                        assert(frame.id != 0);
                        ASSERT_STREAM();
                        auto& prio = static_cast<const PriorityFrame&>(frame);
                        stream->impl->priority = prio.priority;
                        return {};
                    }
                    case FrameType::push_promise: {
                        if (!impl->send.setting[k(SettingKey::enable_push)]) {
                            return UpdateResult{
                                .err = H2Error::protocol,
                                .detail = StreamError::push_promise_disabled,
                                .id = frame.id,
                                .frame = FrameType::push_promise,
                            };
                        }
                        ASSERT_STREAM();
                        if (stream->status() != Status::open && stream->status() != Status::half_closed_remote) {
                            return UpdateResult{
                                .err = H2Error::protocol,
                                .detail = StreamError::not_acceptable_on_current_status,
                                .id = frame.id,
                                .frame = FrameType::push_promise,
                                .status = stream->status(),
                            };
                        }
                        auto& promise = static_cast<const PushPromiseFrame&>(frame);
                        auto new_stream = impl->get_stream(promise.promise);
                        if (new_stream->status() != Status::idle) {
                            return UpdateResult{
                                .err = H2Error::protocol,
                                .detail = StreamError::not_acceptable_on_current_status,
                                .id = promise.promise,
                                .frame = FrameType::push_promise,
                                .status = stream->status(),
                            };
                            ;
                        }
                        new_stream->impl->status = Status::reserved_local;
                        if (!(frame.flag & Flag::end_headers)) {
                            impl->recv.continuous_id = promise.promise;
                        }
                        return {};
                    }
                    default: {
                        return UpdateResult{
                            .err = H2Error::protocol,
                            .detail = StreamError::unsupported_frame,
                            .id = frame.id,
                        };
                    }
                }
            }

        }  // namespace http2
    }      // namespace net
}  // namespace utils
