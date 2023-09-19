/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <platform/windows/dllexport_source.h>
#include <file/platform.h>
#include <platform/detect.h>
#ifdef UTILS_PLATFORM_WINDOWS
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#ifndef UTILS_PLATFORM_WASI
#include <sys/mman.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#ifdef UTILS_PLATFORM_WINDOWS
#define _sopen_s(fd, path, mode, share, perm) ::_wsopen_s(fd, path, mode, share, perm)
#define _close(fd) ::_close(fd)
#undef _stat
#define _stat(path, stat) ::_wstat64(path, stat)
#define _fdopen(fd, mode) _fdopen(fd, mode)
#else
#define _sopen_s(fd, path, mode, share, perm) (*(fd) = ::open(path, mode))
#define _close(fd) ::close(fd)
#define _stat(path, stat_) ::stat(path, stat_)
#define _fdopen(fd, mode) ::fdopen(fd, mode)
#define _O_RDONLY O_RDONLY
#define _SH_DENYWR 0
#define _S_IREAD 0
#endif

using namespace utils::file::platform;

namespace utils {
    namespace file {
        namespace platform {
#ifdef UTILS_PLATFORM_WINDOWS
            static bool try_get_map(ReadFileInfo* info) {
                auto filehandle = reinterpret_cast<::HANDLE>(::_get_osfhandle(info->fd));
                if (filehandle == INVALID_HANDLE_VALUE) {
                    return false;
                }
                info->maphandle = CreateFileMappingW(filehandle, NULL, PAGE_READONLY, 0, 0, NULL);
                if (info->maphandle == nullptr) {
                    return false;
                }
                info->mapptr = reinterpret_cast<char*>(MapViewOfFile(info->maphandle, FILE_MAP_READ, 0, 0, 0));
                if (info->mapptr == nullptr) {
                    ::CloseHandle(info->maphandle);
                    info->maphandle = nullptr;
                    return false;
                }
                return true;
            }

            static void clean_map(ReadFileInfo* info) {
                ::UnmapViewOfFile(info->mapptr);
                info->mapptr = nullptr;
                ::CloseHandle(info->maphandle);
                info->maphandle = nullptr;
            }
#elif defined(UTILS_PLATFORM_WASI)
            static bool try_get_map(ReadFileInfo* info) {
                return false;
            }

            static void clean_map(ReadFileInfo* info) {}
#else
            static bool try_get_map(ReadFileInfo* info) {
                long pagesize = ::getpagesize(), mapsize = 0;
                mapsize = (info->stat.st_size / pagesize + 1) * pagesize;
                info->mapptr = reinterpret_cast<char*>(mmap(nullptr, mapsize, PROT_READ, MAP_SHARED, info->fd, 0));
                if (!info->mapptr) {
                    return false;
                }
                info->maplen = mapsize;
                return true;
            }

            static void clean_map(ReadFileInfo* info) {
                ::munmap(info->mapptr, info->maplen);
                info->mapptr = nullptr;
                info->maplen = 0;
            }
#endif

            static bool open_impl(ReadFileInfo* info, const wrap::path_char* path) {
                _sopen_s(&info->fd, path, _O_RDONLY, _SH_DENYWR, _S_IREAD);
                if (info->fd == -1) {
                    return false;
                }
                auto result = _stat(path, &info->stat);
                if (result != 0) {
                    _close(info->fd);
                    info->fd = -1;
                    return false;
                }
                if (!try_get_map(info)) {
                    info->file = _fdopen(info->fd, "rb");
                    if (!info->file) {
                        _close(info->fd);
                        info->fd = -1;
                        return false;
                    }
                }
                return true;
            }

            bool ReadFileInfo::is_open() const {
                if (this->file || this->fd != ~0) {
                    return true;
                }
                return false;
            }

            bool ReadFileInfo::open(const wrap::path_char* path) {
                if (!path) {
                    return false;
                }
                if (is_open()) {
                    return false;
                }
                if (!open_impl(this, path)) {
                    return false;
                }
                return true;
            }

            void ReadFileInfo::close() {
                if (this->mapptr) {
                    clean_map(this);
                }
                if (this->file) {
                    ::fclose(this->file);
                    this->file = nullptr;
                    this->fd = -1;
                }
                else if (this->fd != -1) {
                    _close(this->fd);
                    this->fd = -1;
                }
                this->stat = ReadFileInfo::stat_type{0};
            }

            ReadFileInfo::~ReadFileInfo() {
                this->close();
            }
        }  // namespace platform
    }      // namespace file
}  // namespace utils
