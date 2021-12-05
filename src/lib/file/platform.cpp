/*license*/

#include "../../include/file/platform.h"
#include <windows.h>
using namespace utils::file::platform;

namespace utils {
    namespace file {
        namespace platform {
#ifdef _WIN32
            static bool open_impl(FileInfo* info, const path_char* path) {
                info->handle = ::CreateFileW(
                    path,
                    GENERIC_READ,
                    FILE_SHARE_READ,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);
                if (info->handle == INVALID_HANDLE_VALUE) {
                    info->handle = nullptr;
                    return false;
                }
            }
#else
            static bool open_impl(FileInfo* info, const path_char* path) {
            }
#endif
            bool FileInfo::is_open() const {
                if (this->file || this->fd != ~0
#ifdef _WIN32
                    || this->handle
#endif
                ) {
                    return true;
                }
                return false;
            }

            bool FileInfo::open(const path_char* path) {
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
