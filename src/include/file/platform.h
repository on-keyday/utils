/*license*/

// platform - platform dependency wrapper
#pragma once

#include <cstdio>

#include <sys/stat.h>

namespace utils {
    namespace file {
        namespace platform {
            struct FileInfo {
                ::FILE* file = nullptr;
                int fd = ~0;
#ifdef _WIN32
                void* handle = nullptr;
                struct ::_stat64 stat;
#else
                struct ::stat stat;
#endif
            };
        }  // namespace platform
    }      // namespace file
}  // namespace utils