/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"

#include "../../../include/net/http2/stream.h"
#include "../../../include/net/http/http1.h"
#include "../../../include/wrap/lite/queue.h"
#include "../../../include/net_util/hpack.h"
#include "../../../include/wrap/lite/hash_map.h"
#include "../http/header_impl.h"
#include "../../../include/number/bitmask.h"
#include "frame_reader.h"

namespace utils {
    namespace net {
        namespace http2 {
            enum class SettingKey {
                table_size = 1,
                enable_push = 2,
                max_concurrent = 3,
                initial_windows_size = 4,
                max_frame_size = 5,
                header_list_size = 6,
            };

            auto k(SettingKey v) {
                return (std::uint16_t)v;
            }

            namespace internal {

                struct StreamImpl {
                    std::int32_t id;
                    Status status;
                    std::int64_t window = 0;
                    net::internal::HeaderImpl h;
                    wrap::string remote_raw;
                    wrap::string local_raw;
                    wrap::string data;
                    Priority priority{0};
                    std::uint32_t code = 0;

                    std::int32_t get_dependency() {
                        return priority.depend & number::msb_off<std::uint32_t>;
                    }

                    bool is_exclusive() {
                        return priority.depend & number::msb_on<std::uint32_t>;
                    }
                };
                using HpackTable = wrap::queue<std::pair<wrap::string, wrap::string>>;

                struct ConnectionImpl {
                    HpackTable encode_table;
                    HpackTable decode_table;
                    std::int64_t window = 0;
                    std::int32_t id_max = 0;
                    std::int32_t preproced_id = 0;
                    FrameType preproced_type = FrameType::settings;
                    std::uint32_t max_table_size = 0;
                    wrap::hash_map<std::int32_t, Stream> streams;
                    wrap::hash_map<std::uint16_t, std::uint32_t> setting;
                    std::int32_t continuous_id = 0;
                    std::int32_t prev_ping = 0;
                    bool server = false;

                    std::int32_t next_stream() const {
                        if (server) {
                            return id_max % 2 == 0 ? id_max + 2 : id_max + 1;
                        }
                        else {
                            return id_max % 2 == 1 ? id_max + 1 : id_max + 2;
                        }
                    }

                    ConnectionImpl() {
                        setting[k(SettingKey::table_size)] = 4096;
                        setting[k(SettingKey::enable_push)] = 1;
                        setting[k(SettingKey::max_concurrent)] = ~0;
                        setting[k(SettingKey::initial_windows_size)] = 65535;
                        setting[k(SettingKey::max_frame_size)] = 16384;
                        setting[k(SettingKey::header_list_size)] = ~0;
                    }

                    Stream* make_new_stream(int id) {
                        auto ptr = &streams[id];
                        ptr->impl = wrap::make_shared<StreamImpl>();
                        ptr->impl->id = id;
                        ptr->impl->status = Status::idle;
                        ptr->impl->window = setting[(std::uint16_t)SettingKey::initial_windows_size];
                    }

                    Stream* get_stream(int id) {
                        if (id <= 0) {
                            return nullptr;
                        }
                        if (id > id_max) {
                            id_max = id;
                            return make_new_stream(id);
                        }
                        auto found = streams.find(id);
                        if (found != streams.end()) {
                            auto& stream = get<1>(*found);
                            if (id < stream.id() && stream.status() == Status::idle) {
                                stream.impl->status = Status::closed;
                            }
                            return &stream;
                        }
                        auto stream = make_new_stream(id);
                        stream->impl->status = Status::closed;
                        return stream;
                    }
                };
            }  // namespace internal

            H2Error decode_hpack(wrap::string& raw, net::internal::HeaderImpl& header, internal::ConnectionImpl& conn) {
                auto err = hpack::decode(raw, conn.decode_table, header, conn.max_table_size);
                if (!err) {
                    return H2Error::compression;
                }
                return H2Error::none;
            }

            struct LastUpdateConnection {
                internal::ConnectionImpl& impl;
                const Frame& frame;
                ~LastUpdateConnection() {
                    impl.preproced_id = frame.id;
                    impl.preproced_type = frame.type;
                }
            };

            H2Error Connection::update_recv(const Frame& frame) {
                auto type = frame.type;
                auto stream = impl->get_stream(frame.id);
                if (frame.len > impl->setting[k(SettingKey::max_frame_size)]) {
                    return H2Error::protocol;
                }
                LastUpdateConnection _{*impl, frame};
                if (impl->continuous_id && frame.type != FrameType::continuous) {
                    return H2Error::protocol;
                }
                switch (type) {
                    case FrameType::data: {
                        assert(frame.id != 0);
                        assert(stream);
                        auto& data = static_cast<const DataFrame&>(frame);
                        stream->impl->data.append(data.data);
                        stream->impl->window -= data.data.size();
                        impl->window -= data.data.size();
                        if (data.flag & Flag::end_stream) {
                            stream->impl->status = Status::half_closed_remote;
                        }
                        return H2Error::none;
                    }
                    case FrameType::header: {
                        assert(frame.id != 0);
                        assert(stream);
                        auto& header = static_cast<const HeaderFrame&>(frame);
                        if (stream->status() != Status::idle) {
                            return H2Error::protocol;
                        }
                        stream->impl->remote_raw.append(header.data);
                        stream->impl->status = Status::open;
                        if (header.flag & Flag::priority) {
                            stream->impl->priority = header.priority;
                        }
                        if (header.flag & Flag::end_stream) {
                            stream->impl->status = Status::half_closed_remote;
                        }
                        if (header.flag & Flag::end_headers) {
                            return decode_hpack(stream->impl->remote_raw, stream->impl->h, *impl);
                        }
                        else {
                            impl->continuous_id = header.id;
                        }
                        return H2Error::none;
                    }
                    case FrameType::continuous: {
                        if (frame.id != impl->preproced_id) {
                            return H2Error::protocol;
                        }
                        assert(stream);
                        auto& cont = static_cast<const Continuation&>(frame);
                        if (impl->continuous_id == frame.id) {
                            stream->impl->remote_raw.append(cont.data);
                            if (cont.flag & Flag::end_headers) {
                                impl->continuous_id = 0;
                                return decode_hpack(stream->impl->remote_raw, stream->impl->h, *impl);
                            }
                        }
                        else {
                            return H2Error::internal;
                            auto to_add = impl->get_stream(impl->continuous_id);
                            stream->impl->local_raw.append(cont.data);
                            if (cont.flag & Flag::end_headers) {
                                impl->continuous_id = 0;
                            }
                            return H2Error::none;
                        }
                        return H2Error::none;
                    }
                    case FrameType::window_update: {
                        auto& window_update = static_cast<const WindowUpdateFrame&>(frame);
                        if (window_update.id == 0) {
                            impl->window += window_update.increment;
                        }
                        else {
                            assert(stream);
                            stream->impl->window += window_update.increment;
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
                        internal::FrameReader<const wrap::string&> r(settings.setting);
                        while (r.pos < r.ref.size()) {
                            std::uint16_t key = 0;
                            std::uint32_t value = 0;
                            if (!r.read(key)) {
                                return H2Error::internal;
                            }
                            if (!r.read(value)) {
                                return H2Error::internal;
                            }
                            auto& current = impl->setting[key];
                            if (key == (std::uint16_t)SettingKey::initial_windows_size) {
                                impl->window = value - current + impl->window;
                                for (auto& stream : impl->streams) {
                                    auto& stwindow = stream.second.impl->window;
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
                    case FrameType::priority: {
                        assert(frame.id != 0);
                        assert(stream);
                        auto& prio = static_cast<const PriorityFrame&>(frame);
                        stream->impl->priority = prio.priority;
                        return H2Error::none;
                    }
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
                    case FrameType::push_promise: {
                        if (!impl->setting[k(SettingKey::enable_push)]) {
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
                        impl->continuous_id = promise.promise;
                        return H2Error::none;
                    }
                    default: {
                        return H2Error::none;
                    }
                }
            }

        }  // namespace http2
    }      // namespace net
}  // namespace utils
