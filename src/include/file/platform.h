/*license*/

// platform - platform dependency wrapper
#pragma once

#include <cstdio>

#include <sys/stat.h>

namespace utils {
    namespace file {
        namespace platform {
#ifdef _WIN32
            using path_char = wchar_t;
#else
            using path_char = char;
#endif
            struct FileInfo {
                ::FILE* file = nullptr;
                int fd = ~0;
#ifdef _WIN32
                void* handle = nullptr;
                struct ::_stat64 stat = {0};
#else
                struct ::stat stat = {0};
#endif
                bool open(const path_char* filename);
                bool from(::FILE* file);
                bool from(int fd);
                bool from(void* handle);
                size_t size() const;
                void close();
                bool is_open() const;
                ~FileInfo();
            };
        }  // namespace platform
    }      // namespace file
}  // namespace utils
