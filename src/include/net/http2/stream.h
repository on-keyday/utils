/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// stream - http2 stream
#pragma once
#include "../../platform/windows/dllexport_header.h"
#include "conn.h"
#include "../http/http1.h"
#include "../../endian/endian.h"
#include "../async/pool.h"

namespace utils {
    namespace net {
        namespace http2 {
            struct Stream;
            namespace internal {
                struct ConnectionImpl;
                struct StreamImpl;
                wrap::shared_ptr<internal::StreamImpl>& get_impl(Stream&);
            }  // namespace internal

            enum class Status {
                idle,
                closed,
                open,
                half_closed_remote,
                half_closed_local,
                reserved_remote,
                reserved_local,
                unknown,
            };

            BEGIN_ENUM_STRING_MSG(Status, status_name)
            ENUM_STRING_MSG(Status::idle, "idle")
            ENUM_STRING_MSG(Status::closed, "closed")
            ENUM_STRING_MSG(Status::open, "open")
            ENUM_STRING_MSG(Status::half_closed_local, "half_closed_local")
            ENUM_STRING_MSG(Status::half_closed_remote, "half_closed_remote")
            ENUM_STRING_MSG(Status::reserved_local, "reserved_local")
            ENUM_STRING_MSG(Status::reserved_remote, "reserved_remote")
            END_ENUM_STRING_MSG("unknown")

            enum class StreamError {
                none,
                over_max_frame_size,
                continuation_not_followed,
                not_acceptable_on_current_status,
                internal_data_read,
                push_promise_disabled,
                unsupported_frame,
                hpack_failed,
                unimplemented,
                ping_maybe_failed,
                writing_frame,
                reading_frame,
                require_id_0,
                require_id_not_0,
                stream_blocked,
            };

            BEGIN_ENUM_STRING_MSG(StreamError, error_msg)
            ENUM_STRING_MSG(StreamError::over_max_frame_size, "frame size is over max_frame_size")
            ENUM_STRING_MSG(StreamError::continuation_not_followed, "expect continuation frame but not")
            ENUM_STRING_MSG(StreamError::not_acceptable_on_current_status, "such type frame is not acceptable on current status")
            ENUM_STRING_MSG(StreamError::internal_data_read, "error while internal data read")
            ENUM_STRING_MSG(StreamError::push_promise_disabled, "push promise frame is disabled but it was recvieved/sent")
            ENUM_STRING_MSG(StreamError::unsupported_frame, "unspported type frame handled")
            ENUM_STRING_MSG(StreamError::hpack_failed, "hpack encode/decode failed")
            ENUM_STRING_MSG(StreamError::unimplemented, "that feature is unimplemented")
            ENUM_STRING_MSG(StreamError::ping_maybe_failed, "ping maybe failed")
            ENUM_STRING_MSG(StreamError::writing_frame, "error while writing frame")
            ENUM_STRING_MSG(StreamError::reading_frame, "error while reading frame")
            ENUM_STRING_MSG(StreamError::require_id_0, "require id 0 frame")
            ENUM_STRING_MSG(StreamError::require_id_not_0, "require non-zero id frame")
            ENUM_STRING_MSG(StreamError::stream_blocked, "stream now blocking")
            END_ENUM_STRING_MSG("none")

            struct Connection;

            struct DLL Stream {
                int id() const;
                std::uint8_t priority() const;
                std::int64_t window_size() const;
                Status status() const;
                http::HttpAsyncResponse response();

                const char* peek_header(const char* key, size_t index = 0) const;

               private:
                friend struct internal::ConnectionImpl;
                friend struct Connection;
                friend wrap::shared_ptr<internal::StreamImpl>& internal::get_impl(Stream&);
                wrap::shared_ptr<internal::StreamImpl> impl;
            };

            struct UpdateResult {
                H2Error err = H2Error::none;
                StreamError detail = StreamError::none;
                std::int32_t id = 0;
                FrameType frame = {};
                Status status = Status::unknown;
                bool stream_level = false;
            };

            namespace internal {
                wrap::shared_ptr<internal::ConnectionImpl>& get_impl(Connection&);
            }

            struct DLL Connection {
                Stream* stream(int id);
                Stream* new_stream();

                UpdateResult update_recv(const Frame& frame);
                UpdateResult update_send(const Frame& frame);
                int errcode();

                std::int32_t max_proced() const;

                bool make_data(std::int32_t id, wrap::string& data, DataFrame& frame, bool& block);
                bool make_header(http::Header&& h, HeaderFrame& frame, wrap::string& remain);
                bool split_header(HeaderFrame& frame, wrap::string& remain);
                bool make_continuous(std::int32_t id, wrap::string& remain, Continuation& frame);
                Connection();

                template <class Set>
                bool make_settings(SettingsFrame& frame, Set&& settings) {
                    frame.id = 0;
                    frame.type = FrameType::settings;
                    frame.flag = Flag::none;
                    frame.len = 0;
                    for (auto& s : settings) {
                        auto key = get<0>(s);
                        auto value = get<1>(s);
                        static_assert(sizeof(key) == 2, "require 2 octect value for http2 settings key");
                        static_assert(sizeof(value) == 4, "require 4 octect value for http2 settings value");
                        key = endian::to_network(&key);
                        value = endian::to_network(&value);
                        const char* kptr = reinterpret_cast<const char*>(&key);
                        const char* vptr = reinterpret_cast<const char*>(&value);
                        frame.setting.append(kptr, 2);
                        frame.setting.append(vptr, 4);
                        frame.len += 6;
                    }
                    return true;
                }

               private:
                friend wrap::shared_ptr<internal::ConnectionImpl>& internal::get_impl(Connection&);
                wrap::shared_ptr<internal::ConnectionImpl> impl;
            };

            struct ReadResult {
                UpdateResult err;
                wrap::shared_ptr<Frame> frame;
            };

            struct DLL Context {
                Connection state;
                wrap::shared_ptr<Conn> io;
                async::Future<UpdateResult> write(const Frame& frame);
                async::Future<ReadResult> read();

                UpdateResult write(async::Context& ctx, const Frame& frame);
                ReadResult read(async::Context& ctx);

                UpdateResult serialize_frame(IBuffer buf, const Frame& frame);
                UpdateResult write_serial(async::Context& ctx, const wrap::string& buf);
            };

            struct NegotiateResult {
                UpdateResult err;
                wrap::shared_ptr<Context> ctx;
            };

            DLL NegotiateResult STDCALL negotiate(async::Context& ctx, wrap::shared_ptr<Conn>&& conn, SettingsFrame& frame);
            DLL async::Future<NegotiateResult> STDCALL negotiate(wrap::shared_ptr<Conn>&& conn, SettingsFrame& frame);

            template <class Settings>
            NegotiateResult negotiate(async::Context& ctx, wrap::shared_ptr<Conn>&& conn, Settings& setting) {
                SettingsFrame frame{0};
                frame.type = FrameType::settings;
                for (auto& s : setting) {
                    auto key = get<0>(s);
                    auto value = get<1>(s);
                    static_assert(sizeof(key) == 2, "require 2 octect value for http2 settings key");
                    static_assert(sizeof(value) == 4, "require 4 octect value for http2 settings value");
                    key = endian::to_network(&key);
                    value = endian::to_network(&value);
                    const char* kptr = reinterpret_cast<const char*>(&key);
                    const char* vptr = reinterpret_cast<const char*>(&value);
                    frame.setting.append(kptr, 2);
                    frame.setting.append(vptr, 4);
                    frame.len += 6;
                }
                return negotiate(ctx, std::move(conn), frame);
            }

            template <class Settings>
            async::Future<NegotiateResult> negotiate(wrap::shared_ptr<Conn>&& conn, Settings& setting) {
                return net::start([&](async::Context& ctx) {
                    return negotiate(ctx, std::move(conn), setting);
                });
            }

            enum class SettingKey : std::uint16_t {
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

        }  // namespace http2
    }      // namespace net
}  // namespace utils
