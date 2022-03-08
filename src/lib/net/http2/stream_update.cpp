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

            H2Error decode_hpack(wrap::string& raw, net::internal::HeaderImpl& header, internal::ConnectionImpl& conn) {
                auto err = hpack::decode(raw, conn.recv.decode_table, header, conn.max_table_size);
                if (!err) {
                    return H2Error::compression;
                }
                return H2Error::none;
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

            H2Error Connection::update_recv(const Frame& frame) {
                auto stream = impl->get_stream(frame.id);
                if (frame.len > impl->recv.setting[k(SettingKey::max_frame_size)]) {
                    return H2Error::protocol;
                }
                LastUpdateConnection _{impl->recv, frame};
                if (impl->recv.continuous_id && frame.type != FrameType::continuous) {
                    return H2Error::protocol;
                }
                switch (frame.type) {
                    case FrameType::ping: {
                        assert(frame.id == 0);
                        assert(stream);
                        auto& ping = static_cast<const PingFrame&>(frame);
                        if (ping.flag & Flag::ack) {
                            if (ping.opeque != impl->prev_ping) {
                                return H2Error::ping_failed;
                            }
                        }
                        return H2Error::none;
                    }
                    case FrameType::data: {
                        assert(frame.id != 0);
                        assert(stream);
                        if (stream->status() != Status::open && stream->status() != Status::half_closed_local) {
                            return H2Error::protocol;
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
                        return H2Error::none;
                    }
                    case FrameType::header: {
                        assert(frame.id != 0);
                        assert(stream);
                        auto& header = static_cast<const HeaderFrame&>(frame);
                        if (stream->status() != Status::idle && stream->status() != Status::reserved_remote) {
                            return H2Error::protocol;
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
                        return H2Error::none;
                    }
                    case FrameType::continuous: {
                        if (frame.id != impl->recv.preproced_id) {
                            return H2Error::protocol;
                        }
                        assert(stream);
                        auto& cont = static_cast<const Continuation&>(frame);
                        if (impl->recv.continuous_id == frame.id) {
                            stream->impl->remote_raw.append(cont.data);
                            if (cont.flag & Flag::end_headers) {
                                impl->recv.continuous_id = 0;
                                return decode_hpack(stream->impl->remote_raw, stream->impl->h, *impl);
                            }
                        }
                        else {
                            return H2Error::internal;  // unsafe implementation
                            auto to_add = impl->get_stream(impl->recv.continuous_id);
                            to_add->impl->local_raw.append(cont.data);
                            if (cont.flag & Flag::end_headers) {
                                impl->recv.continuous_id = 0;
                            }
                            return H2Error::none;
                        }
                        return H2Error::none;
                    }
                    case FrameType::window_update: {
                        auto& window_update = static_cast<const WindowUpdateFrame&>(frame);
                        if (window_update.id == 0) {
                            impl->send.window += window_update.increment;
                        }
                        else {
                            assert(stream);
                            stream->impl->send_window += window_update.increment;
                        }
                        return H2Error::none;
                    }
                    case FrameType::settings: {
                        assert(frame.id == 0);
                        assert(frame.len % 6 == 0);
                        if (frame.flag & Flag::ack) {
                            return H2Error::none;
                        }
                        auto& settings = static_cast<const SettingsFrame&>(frame);
                        internal::FrameReader<const wrap::string&> r{settings.setting};
                        while (r.pos < r.ref.size()) {
                            std::uint16_t key = 0;
                            std::uint32_t value = 0;
                            if (!r.read(key)) {
                                return H2Error::internal;
                            }
                            if (!r.read(value)) {
                                return H2Error::internal;
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
                        return H2Error::none;
                    }
                    case FrameType::rst_stream: {
                        assert(frame.id != 0);
                        assert(stream);
                        if (stream->status() == Status::idle) {
                            return H2Error::protocol;
                        }
                        auto& rst = static_cast<const RstStreamFrame&>(frame);
                        stream->impl->code = rst.code;
                        stream->impl->status = Status::closed;
                        return H2Error::none;
                    }
                    case FrameType::goaway: {
                        assert(frame.id == 0);
                        auto& goaway = static_cast<const GoAwayFrame&>(frame);
                        impl->code = goaway.code;
                        impl->debug_data = goaway.data;
                        return H2Error::none;
                    }
                    case FrameType::priority: {
                        assert(frame.id != 0);
                        assert(stream);
                        auto& prio = static_cast<const PriorityFrame&>(frame);
                        stream->impl->priority = prio.priority;
                        return H2Error::none;
                    }
                    case FrameType::push_promise: {
                        if (!impl->recv.setting[k(SettingKey::enable_push)]) {
                            return H2Error::protocol;
                        }
                        assert(frame.id != 0);
                        assert(stream);
                        if (stream->status() != Status::open && stream->status() != Status::half_closed_remote) {
                            return H2Error::protocol;
                        }
                        auto& promise = static_cast<const PushPromiseFrame&>(frame);
                        auto new_stream = impl->get_stream(promise.promise);
                        if (new_stream->status() != Status::idle) {
                            return H2Error::protocol;
                        }
                        new_stream->impl->status = Status::reserved_remote;
                        if (!(frame.flag & Flag::end_headers)) {
                            impl->recv.continuous_id = promise.promise;
                        }
                        return H2Error::none;
                    }
                    default: {
                        return H2Error::none;
                    }
                }
            }

            H2Error Connection::update_send(const Frame& frame) {
                if (frame.len > impl->recv.setting[k(SettingKey::max_frame_size)]) {
                    return H2Error::protocol;
                }
                LastUpdateConnection _{impl->send, frame};
                auto stream = impl->get_stream(frame.id);
                if (impl->send.continuous_id && frame.type != FrameType::continuous) {
                    return H2Error::protocol;
                }
                switch (frame.type) {
                    case FrameType::data: {
                        assert(stream);
                        if (stream->status() != Status::open && stream->status() != Status::half_closed_remote) {
                            return H2Error::protocol;
                        }
                        stream->impl->send_window -= frame.len;
                        if (frame.flag & Flag::end_stream) {
                            if (stream->status() == Status::half_closed_remote) {
                                stream->impl->status = Status::closed;
                            }
                            else {
                                stream->impl->status = Status::half_closed_local;
                            }
                        }
                        return H2Error::none;
                    }
                    case FrameType::header: {
                        assert(stream);
                        if (stream->status() != Status::idle && stream->status() != Status::reserved_local) {
                            return H2Error::protocol;
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
                        return H2Error::none;
                    }
                    case FrameType::continuous: {
                        if (frame.id != impl->send.continuous_id) {
                            return H2Error::protocol;
                        }
                        if (frame.flag & Flag::end_headers) {
                            impl->send.continuous_id = 0;
                        }
                        return H2Error::none;
                    }
                    case FrameType::window_update: {
                        auto& update = static_cast<const WindowUpdateFrame&>(frame);
                        if (frame.id == 0) {
                            impl->recv.window += update.increment;
                        }
                        else {
                            stream->impl->recv_window += update.increment;
                        }
                        return H2Error::none;
                    }
                    case FrameType::rst_stream: {
                        auto& rst = static_cast<const RstStreamFrame&>(frame);
                        stream->impl->code = rst.code;
                        stream->impl->status = Status::closed;
                        return H2Error::none;
                    }
                    case FrameType::goaway: {
                        auto& goaway = static_cast<const GoAwayFrame&>(frame);
                        impl->code = goaway.code;
                        impl->debug_data = goaway.data;
                    }
                    case FrameType::settings: {
                        assert(frame.id == 0);
                        assert(frame.len % 6 == 0);
                        if (frame.flag & Flag::ack) {
                            return H2Error::none;
                        }
                        auto& settings = static_cast<const SettingsFrame&>(frame);
                        internal::FrameReader<const wrap::string&> r{settings.setting};
                        while (r.pos < r.ref.size()) {
                            std::uint16_t key = 0;
                            std::uint32_t value = 0;
                            if (!r.read(key)) {
                                return H2Error::internal;
                            }
                            if (!r.read(value)) {
                                return H2Error::internal;
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
                        return H2Error::none;
                    }
                    case FrameType::priority: {
                        assert(frame.id != 0);
                        assert(stream);
                        auto& prio = static_cast<const PriorityFrame&>(frame);
                        stream->impl->priority = prio.priority;
                        return H2Error::none;
                    }
                    case FrameType::push_promise: {
                        if (!impl->send.setting[k(SettingKey::enable_push)]) {
                            return H2Error::protocol;
                        }
                        assert(stream);
                        if (stream->status() != Status::open && stream->status() != Status::half_closed_remote) {
                            return H2Error::protocol;
                        }
                        auto& promise = static_cast<const PushPromiseFrame&>(frame);
                        auto new_stream = impl->get_stream(promise.promise);
                        if (new_stream->status() != Status::idle) {
                            return H2Error::protocol;
                        }
                        new_stream->impl->status = Status::reserved_local;
                        if (!(frame.flag & Flag::end_headers)) {
                            impl->recv.continuous_id = promise.promise;
                        }
                        return H2Error::none;
                    }
                    default: {
                        return H2Error::internal;
                    }
                }
            }

        }  // namespace http2
    }      // namespace net
}  // namespace utils
