/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// h2settings - http2 settings util
#pragma once
#include <helper/pushbacker.h>

namespace utils {
    namespace dnet {
        namespace h2frame {
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
        }  // namespace h2frame

        namespace h2set {
            using Skey = utils::net::http2::SettingKey;
            using Vec = helper::CharVecPushbacker<char>;
            struct PredefinedSettings {
                std::uint32_t header_table_size = 4096;
                bool enable_push = true;
                std::uint32_t max_concurrent_stream = ~0;
                std::uint32_t initial_window_size = 65535;
                std::uint32_t max_frame_size = 16384;
                std::uint32_t max_header_list_size = ~0;
            };

            constexpr bool write_key_value(Vec& vec, std::uint16_t key, std::uint32_t value) {
                vec.push_back((key >> 8) & 0xff);
                vec.push_back(key & 0xff);
                vec.push_back((value >> 24) & 0xff);
                vec.push_back((value >> 16) & 0xff);
                vec.push_back((value >> 8) & 0xff);
                vec.push_back(value & 0xff);
                return !vec.overflow;
            }

            constexpr bool write_predefined_settings(Vec& vec, PredefinedSettings& settings, bool only_not_default = false) {
                constexpr auto default_ = PredefinedSettings();
                using sk = Skey;
                auto write = [&](std::uint16_t key, std::uint32_t value) {
                    write_key_value(vec, key, value);
                };
                if (only_not_default) {
#define WRITE_IF(key, value)                    \
    if ((settings.value) != (default_.value)) { \
        write(key, settings.value);             \
    }
                    WRITE_IF(k(sk::table_size), header_table_size);
                    WRITE_IF(k(sk::enable_push), enable_push ? 1 : 0);
                    WRITE_IF(k(sk::max_concurrent), max_concurrent_stream);
                    WRITE_IF(k(sk::initial_windows_size), initial_window_size);
                    WRITE_IF(k(sk::max_frame_size), max_frame_size);
                    WRITE_IF(k(sk::header_list_size), max_header_list_size);
#undef WRITE_IF
                }
                else {
                    write(k(sk::table_size), settings.header_table_size);
                    write(k(sk::enable_push), settings.enable_push ? 1 : 0);
                    write(k(sk::max_concurrent), settings.max_concurrent_stream);
                    write(k(sk::initial_windows_size), settings.initial_window_size);
                    write(k(sk::max_frame_size), settings.max_frame_size);
                    write(k(sk::header_list_size), settings.max_header_list_size);
                }
                return !vec.overflow;
            }

            constexpr bool write_settings(Vec& vec, auto&& settings, bool only_not_default = false) {
                using set_t = std::decay_t<std::remove_cvref_t<decltype(settings)>>;
                for (auto&& setting : settings) {
                    auto key = std::uint16_t(get<0>(setting));
                    auto value = std::uint32_t(get<1>(setting));
                    write_key_value(vec, key, value);
                }
                return !vec.overflow;
            }

            constexpr bool read_settings(auto&& settings, size_t len, auto&& set) {
                if constexpr (std::is_pointer_v<std::decay_t<decltype(settings)>>) {
                    if (!settings) {
                        return true;
                    }
                }
                if (len % 6) {
                    return false;
                }
                size_t index = 0;
                auto sts = [&](int i) {
                    return std::uint32_t(std::uint8_t(settings[index + i]));
                };
                while (index < len) {
                    auto key = sts(0) << 8 | sts(1);
                    auto value = sts(2) << 24 | sts(3) << 16 | sts(4) << 8 | sts(5);
                    set(key, value);
                    index += 6;
                }
                return true;
            }

        }  // namespace h2set
    }      // namespace dnet
}  // namespace utils
