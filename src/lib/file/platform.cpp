/*license*/

#include "../../include/file/platform.h"
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

using namespace utils::file::platform;

namespace utils {
    namespace file {
        namespace platform {
#ifdef _WIN32
            static bool try_get_map(ReadFileInfo* info) {
                auto filehandle = reinterpret_cast<::HANDLE>(::_get_osfhandle(info->fd));
                if (filehandle == INVALID_HANDLE_VALUE) {
                    return false;
                }
                info->maphandle = CreateFileMappingA(filehandle, NULL, PAGE_READONLY, 0, 0, NULL);
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
#else
            static bool try_get_map(ReadFileInfo* info) {
                long pagesize = ::getpagesize(), mapsize = 0;
                mapsize = (info->stat.st_size / pagesize + 1) * pagesize;
                info->mapptr = reinterpret_cast<char*>(mmap(nullptr, mapsize, PROT_READ, MAP_SHARED, info->fd, 0));
                if (!info->mapptr) {
                    return false;
                }
                return true;
            }
#endif
            static bool open_impl(ReadFileInfo* info, const path_char* path) {
#ifdef _WIN32
                info->fd = ::_wopen(path, _O_RDONLY);
                if (info->fd == -1) {
                    return false;
                }
                auto result = ::_wstat64(path, &info->stat);
                if (result != 0) {
                    ::_close(info->fd);
                    info->fd = -1;
                }
#else
                info->fd = ::open(path, O_RDONLY);
                if (info->fd == -1) {
                    return false;
                }
                auto reuslt = ::stat(path, &info->stat);
                if (result != 0) {
                    ::_close(info->fd);
                    info->fd = -1;
                }
#endif

                return true;
            }

            bool ReadFileInfo::is_open() const {
                if (this->file || this->fd != ~0) {
                    return true;
                }
                return false;
            }

            bool ReadFileInfo::open(const path_char* path) {
                if (!path) {
                    return false;
                }
                if (is_open()) {
                    return false;
                }
            }
        }  // namespace platform
    }      // namespace file
}  // namespace utils
