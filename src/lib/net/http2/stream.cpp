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

namespace utils {
    namespace net {
        namespace http2 {
            namespace internal {
                struct StreamImpl {
                    std::int32_t id;
                    Status status;
                    std::int64_t window;
                    net::internal::HeaderImpl h;
                    wrap::string header_raw;
                    wrap::string data;
                    Priority priority{0};

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
                    std::int64_t window;
                    std::int32_t id_max = 0;
                    std::int32_t preproced = 0;
                    std::uint32_t max_table_size = 0;
                    wrap::hash_map<std::int32_t, Stream> streams;
                    wrap::hash_map<std::uint16_t, std::uint32_t> setting;
                    bool continuation_mode = false;
                    Stream* make_new_stream(int id) {
                        auto ptr = &streams[id];
                        ptr->impl = wrap::make_shared<StreamImpl>();
                        ptr->impl->id = id;
                        ptr->impl->status = Status::idle;
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

            H2Error decode_hpack(internal::ConnectionImpl& conn, internal::StreamImpl& stream) {
                auto err = hpack::decode(stream.header_raw, conn.decode_table, stream.h, conn.max_table_size);
                if (!err) {
                    return H2Error::compression;
                }
                return H2Error::none;
            }

            struct LastUpdateConnection {
                internal::ConnectionImpl& impl;
                const Frame& frame;
                ~LastUpdateConnection() {
                    impl.preproced = frame.id;
                }
            };

            H2Error Connection::update(const Frame& frame) {
                auto type = frame.type;
                auto stream = impl->get_stream(frame.id);
                LastUpdateConnection _{*impl, frame};
                if (impl->continuation_mode && frame.type != FrameType::continuous) {
                    return H2Error::protocol;
                }
                switch (type) {
                    case FrameType::data: {
                        assert(frame.id != 0);
                        assert(stream);
                        auto& data = static_cast<const DataFrame&>(frame);
                        stream->impl->data.append(data.data);
                        return H2Error::none;
                    }
                    case FrameType::header: {
                        assert(frame.id != 0);
                        assert(stream);
                        auto& header = static_cast<const HeaderFrame&>(frame);
                        if (stream->status() != Status::idle) {
                            return H2Error::protocol;
                        }
                        stream->impl->header_raw.append(header.data);
                        stream->impl->status = Status::open;
                        if (header.flag & Flag::priority) {
                            stream->impl->priority = header.priority;
                        }
                        if (header.flag & Flag::end_stream) {
                            stream->impl->status = Status::half_closed_remote;
                        }
                        if (header.flag & Flag::end_headers) {
                            return decode_hpack(*impl, *stream->impl);
                        }
                        else {
                            impl->continuation_mode = true;
                        }
                        return H2Error::none;
                    }
                    case FrameType::continuous: {
                        if (frame.id != impl->preproced) {
                            return H2Error::protocol;
                        }
                        assert(stream);
                        auto& cont = static_cast<const Continuation&>(frame);
                        stream->impl->header_raw.append(cont.data);
                        if (cont.flag & Flag::end_headers) {
                            impl->continuation_mode = false;
                            return decode_hpack(*impl, *stream->impl);
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
                    }
                }
            }

        }  // namespace http2
    }      // namespace net
}  // namespace utils