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

namespace utils {
    namespace net {
        namespace http2 {
            namespace internal {
                struct ConnectionImpl;
                struct StreamImpl;

            }  // namespace internal

            enum class Status {
                idle,
                closed,
                open,
                half_closed_remote,
                half_closed_local,
                reserved_remote,
                reserved_local,
            };

            enum class StreamError {
                none,
                invalid_status,
                setting_was_ignored,
            };

            struct Connection;

            struct DLL Stream {
                int id() const;
                std::uint8_t priority() const;
                std::int64_t window_size() const;
                Status status() const;
                http::HttpAsyncResponse response();

               private:
                friend struct internal::ConnectionImpl;
                friend struct Connection;
                wrap::shared_ptr<internal::StreamImpl> impl;
            };

            struct DLL Connection {
                Stream* stream(int id);
                Stream* new_stream();

                H2Error update_recv(const Frame& frame);
                H2Error update_send(const Frame& frame);

                bool make_data(std::int32_t id, wrap::string& data, DataFrame& frame, bool& block);
                bool make_header(http::Header&& h, HeaderFrame& frame, wrap::string& remain);
                bool make_continuous(std::int32_t id, wrap::string& remain, Continuation& frame);

               private:
                wrap::shared_ptr<internal::ConnectionImpl> impl;
            };

            struct DLL Context {
                Connection state;
                wrap::shared_ptr<Conn> io;
                async::Future<H2Error> write(const Frame& frame);
                async::Future<wrap::shared_ptr<Frame>> read();
            };

            DLL async::Future<wrap::shared_ptr<Context>> STDCALL negotiate(wrap::shared_ptr<Conn>&& conn, SettingsFrame& frame);
            template <class Settings>
            async::Future<wrap::shared_ptr<Context>> negotiate(wrap::shared_ptr<Conn>&& conn, Settings& setting) {
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
                return negotiate(std::move(conn), frame);
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
