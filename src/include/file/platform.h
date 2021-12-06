/*license*/

// platform - platform dependency wrapper
#pragma once

#include <cstdio>

#include <sys/stat.h>

#include "../wrap/lite/char.h"

namespace utils {
    namespace file {
        namespace platform {

            struct ReadFileInfo {
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
                bool open(const wrap::path_char* filename);
                void close();
                bool is_open() const;
                ~ReadFileInfo();
            };
        }  // namespace platform
    }      // namespace file
}  // namespace utils
