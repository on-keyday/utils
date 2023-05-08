/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// platform - platform dependency wrapper
// need to link libutils
#pragma once

#include "../platform/windows/dllexport_header.h"

#include <cstdio>

#include <sys/stat.h>

#include "../wrap/light/char.h"
#include <utility>

namespace utils {
    namespace file {
        namespace platform {

            struct DLL ReadFileInfo {
                ::FILE* file = nullptr;
                int fd = -1;
                char* mapptr = nullptr;
#ifdef _WIN32
                using stat_type = struct ::_stat64;
                void* maphandle = nullptr;
#else
                using stat_type = struct ::stat;
                long maplen = 0;
#endif
                stat_type stat = {0};
                constexpr ReadFileInfo() = default;
                constexpr ReadFileInfo& operator=(ReadFileInfo&& in) {
                    if (this == &in) {
                        return *this;
                    }
                    file = std::exchange(in.file, nullptr);
                    fd = std::exchange(in.fd, -1);
                    mapptr = std::exchange(in.mapptr, nullptr);
#ifdef _WIN32
                    maphandle = std::exchange(in.maphandle, nullptr);
#else
                    maplen = std::exchange(in.maplen, 0);
#endif
                    stat = std::exchange(in.stat, stat_type{});
                    return *this;
                }
                bool open(const wrap::path_char* filename);
                void close();
                bool is_open() const;
                ~ReadFileInfo();
            };
        }  // namespace platform
    }      // namespace file
}  // namespace utils
