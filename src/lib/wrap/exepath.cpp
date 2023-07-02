/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/windows/dllexport_source.h>
#include <wrap/exepath.h>
#include <strutil/append.h>
#include <unicode/utf/convert.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace utils {
    namespace wrap {
#ifdef _WIN32
        void STDCALL get_exepath(helper::IPushBacker<> pb) {
            {
                wchar_t buf[1024]{};
                auto len = GetModuleFileNameW(nullptr, buf, sizeof(buf));
                if (len < 1024) {
                    utf::convert(buf, pb);
                    return;
                }
            }
            wrap::path_string path;
            path.resize(2048);
            while (true) {
                auto len = GetModuleFileNameW(nullptr, path.data(), path.size());
                if (len == path.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                    path.resize(path.size() << 1);
                    continue;
                }
                break;
            }
            utf::convert(path.c_str(), pb);
        }
#else
        void get_exepath(helper::IPushBacker<> pb) {
            struct stat64 st;
            constexpr auto proc = "/proc/self/exe";
            if (lstat64(proc, &st) != 0) {
                return;
            }
            if (st.st_size < 1024) {
                {
                    char buf[1024]{};
                    auto red = readlink(proc, buf, 1024);
                    if (red == st.st_size) {
                        strutil::append(pb, buf);
                        return;
                    }
                }
            }
            wrap::path_string path;
            path.resize(st.st_size);
            auto red = readlink(proc, path.data(), st.st_size);
            if (red == st.st_size) {
                strutil::append(pb, path.c_str());
            }
        }
#endif
    }  // namespace wrap
}  // namespace utils
