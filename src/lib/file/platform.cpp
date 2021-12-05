/*license*/

#include "../../include/file/platform.h"
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
#define _sopen_s(fd, path, mode, share, perm) ::_wsopen_s(fd, path, mode, share, perm)
#define _close(fd) ::_close(fd)
#undef _stat
#define _stat(path, stat) ::_wstat64(path, stat)
#define _fdopen(fd, mode) _fdopen(fd, mode)
#else
#define sopen_s(fd, path, mode, share, perm) (*(fd) = ::open(path, mode))
#define _close(fd) ::close(fd)
#define _stat(path, stat) stat(path, stat)
#define _fdopen(fd, mode) fdopen(fd, mode)
#define _O_RDONLY O_RDONLY
#define _SH_DENYWR 0
#define _S_IREAD 0
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

            bool ReadFileInfo::open(const path_char* path) {
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
        }  // namespace platform
    }      // namespace file
}  // namespace utils
