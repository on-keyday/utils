/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <memory>
#include <set>
#include <cstdint>
#include <dnet/dll/allocator.h>
#include <dnet/httpstring.h>
#include <nquic/frame/types.h>
#include <nquic/packet/packet_head.h>
#include <vector>

namespace utils {
    namespace quic {
        namespace state {

            struct StreamID {
               private:
                std::uint64_t value_;

               public:
                constexpr StreamID(std::uint64_t id)
                    : value_(id) {}

                constexpr StreamID(const StreamID&) = default;
                constexpr StreamID& operator=(const StreamID&) = default;

                constexpr auto operator<=>(const StreamID&) const = default;
                constexpr bool is_uni() const {
                    return value_ & 0x02;
                }
                constexpr bool is_from_server() const {
                    return value_ & 0x01;
                }

                const std::uint64_t& value() const {
                    return value_;
                }
            };

            struct IDManager {
                std::uint64_t src;
                StreamID maximum_id;

                bool create(StreamID& id, bool server, bool uni) {
                    auto type = (server ? 0x01 : 0x00) | (uni ? 0x02 : 0x00);
                    if (src & (std::uint64_t(1) << 63) ||
                        src & (std::uint64_t(1) << 62)) {
                        return false;
                    }
                    auto val = src << 2;
                    id = {val | type};
                    src++;
                    return true;
                }
            };

            struct Flow {
                std::int64_t flow;
            };

            enum class StreamStateCode {
                ready,
                send,
                data_sent,
                data_recvd,
                reset_sent,
                reset_recvd,
                recv,
                size_known,
                data_read,
                reset_read,
            };

            struct StreamState {
                StreamStateCode code = StreamStateCode::ready;
                bool sendable_on_reciver(frame::types typ) const {
                    using frame::types;
                    if (code == StreamStateCode::recv) {
                        return typ == types::MAX_STREAM_DATA;
                    }
                    if (typ == types::STOP_SENDING) {
                        return true;
                    }
                    return false;
                }

                bool sendable_on_sender(frame::types typ) const {
                    using frame::types;
                    if (typ == types::RESET_STREAM) {
                        return true;
                    }
                    if (code == StreamStateCode::reset_sent ||
                        code == StreamStateCode::data_sent) {
                        return false;
                    }
                    if (frame::is_STREAM(typ) ||
                        typ == types::STREAM_DATA_BLOCKED) {
                        return true;
                    }
                    return false;
                }

                void send(frame::types typ) {
                    if (code != StreamStateCode::data_recvd &&
                        code != StreamStateCode::reset_recvd &&
                        typ == frame::types::RESET_STREAM) {
                        code = StreamStateCode::reset_sent;
                    }
                    if (code == StreamStateCode::ready) {
                        if (frame::is_STREAM(typ) ||
                            typ == frame::types::STREAM_DATA_BLOCKED) {
                            code = StreamStateCode::send;
                        }
                        else if (typ == frame::types::RESET_STREAM) {
                            code = StreamStateCode::reset_sent;
                        }
                    }
                    if (code == StreamStateCode::send) {
                        if (frame::is_STREAM(typ) &&
                            int(typ) & frame::bits::FIN) {
                            code = StreamStateCode::data_sent;
                        }
                    }
                }

                void recv(frame::types typ) {
                    if (code != StreamStateCode::data_read &&
                        code != StreamStateCode::reset_read &&
                        typ == frame::types::RESET_STREAM) {
                        code = StreamStateCode::reset_sent;
                    }
                    if (code == StreamStateCode::recv) {
                        if (frame::is_STREAM(typ) &&
                            int(typ) & frame::bits::FIN) {
                            code = StreamStateCode::size_known;
                        }
                    }
                }
            };

            template <class T>
            using Vec = std::vector<T, dnet::glheap_allocator<T>>;

            struct PartialData {
                std::uint64_t offset;
                dnet::String data;
            };

            struct Stream {
                StreamID id;
                bool sender = false;
                StreamState send;
                StreamState recv{StreamStateCode::recv};
                Flow flow;
                Vec<PartialData> recv_data;
            };

            using StreamSet = std::set<std::shared_ptr<Stream>,
                                       std::less<>,
                                       dnet::glheap_allocator<std::shared_ptr<Stream>>>;

            struct ConnID {
                dnet::String value;
            };

            struct Conn {
                IDManager idsrc;
                StreamSet streams;
                Flow flow;
            };
        }  // namespace state
    }      // namespace quic
}  // namespace utils
