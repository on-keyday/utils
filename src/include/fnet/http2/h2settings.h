/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// h2settings - http2 settings util
#pragma once
#include <helper/pushbacker.h>

namespace futils {
    namespace fnet::http2 {
        namespace setting {
            enum class SettingKey : std::uint16_t {
                max_header_table_size = 1,
                enable_push = 2,
                max_concurrent = 3,
                initial_windows_size = 4,
                max_frame_size = 5,
                header_list_size = 6,
            };

            constexpr auto k(SettingKey v) {
                return (std::uint16_t)v;
            }

            using Vec = helper::CharVecPushbacker<char>;
            struct PredefinedSettings {
                std::uint32_t max_header_table_size = 4096;
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
                using sk = SettingKey;
                auto write = [&](std::uint16_t key, std::uint32_t value) {
                    write_key_value(vec, key, value);
                };
                if (only_not_default) {
#define WRITE_IF(key, value)                    \
    if ((settings.value) != (default_.value)) { \
        write(key, settings.value);             \
    }
                    WRITE_IF(k(sk::max_header_table_size), max_header_table_size);
                    WRITE_IF(k(sk::enable_push), enable_push ? 1 : 0);
                    WRITE_IF(k(sk::max_concurrent), max_concurrent_stream);
                    WRITE_IF(k(sk::initial_windows_size), initial_window_size);
                    WRITE_IF(k(sk::max_frame_size), max_frame_size);
                    WRITE_IF(k(sk::header_list_size), max_header_list_size);
#undef WRITE_IF
                }
                else {
                    write(k(sk::max_header_table_size), settings.max_header_table_size);
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

        }  // namespace setting

    }  // namespace fnet::http2
}  // namespace futils