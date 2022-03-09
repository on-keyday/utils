/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// stream_impl - stream impl
#pragma once
#include "../../../include/net/http2/stream.h"
#include "../../../include/wrap/lite/hash_map.h"
#include "../http/header_impl.h"
#include "../../../include/number/bitmask.h"
#include "../../../include/wrap/lite/queue.h"

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

            constexpr auto k(SettingKey v) {
                return (std::uint16_t)v;
            }

            namespace internal {

                struct StreamImpl {
                    std::int32_t id;
                    Status status;
                    std::int64_t recv_window = 0;
                    std::int64_t send_window = 0;
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
                using Settings = wrap::hash_map<std::uint16_t, std::uint32_t>;

                struct RecvState {
                    HpackTable decode_table;
                    std::int64_t window = 0;
                    std::int32_t preproced_id = 0;
                    FrameType preproced_type = FrameType::settings;
                    std::int32_t continuous_id = 0;
                    Settings setting;
                };

                struct SendState {
                    HpackTable encode_table;
                    std::uint32_t window = 0;
                    std::int32_t preproced_id = 0;
                    FrameType preproced_type = FrameType::settings;
                    std::int32_t continuous_id = 0;
                    Settings setting;
                };

                void initial_setting(Settings& setting) {
                    setting[k(SettingKey::table_size)] = 4096;
                    setting[k(SettingKey::enable_push)] = 1;
                    setting[k(SettingKey::max_concurrent)] = ~0;
                    setting[k(SettingKey::initial_windows_size)] = 65535;
                    setting[k(SettingKey::max_frame_size)] = 16384;
                    setting[k(SettingKey::header_list_size)] = ~0;
                }

                struct ConnectionImpl {
                    SendState send;
                    RecvState recv;
                    std::uint32_t code = 0;
                    std::int32_t id_max = 0;

                    std::uint32_t max_table_size = 0;
                    wrap::hash_map<std::int32_t, Stream> streams;
                    wrap::string debug_data;

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
                        initial_setting(recv.setting);
                        initial_setting(send.setting);
                    }

                    Stream* make_new_stream(int id) {
                        auto ptr = &streams[id];
                        ptr->impl = wrap::make_shared<StreamImpl>();
                        ptr->impl->id = id;
                        ptr->impl->status = Status::idle;
                        ptr->impl->send_window = send.setting[(std::uint16_t)SettingKey::initial_windows_size];
                        ptr->impl->recv_window = recv.setting[(std::uint16_t)SettingKey::initial_windows_size];
                        return ptr;
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
        }      // namespace http2
    }          // namespace net
}  // namespace utils