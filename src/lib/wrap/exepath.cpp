/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/windows/dllexport_source.h>
#include <wrap/exepath.h>
#include <strutil/append.h>
#include <unicode/utf/convert.h>
#include <platform/detect.h>
#ifdef FUTILS_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace futils {
    namespace wrap {
#ifdef FUTILS_PLATFORM_WINDOWS
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
            constexpr auto proc = "/proc/self/exe";
            {  // first time
                char buf[1024]{};
                auto red = readlink(proc, buf, 1024);
                if (red < 1024) {
                    strutil::append(pb, buf);
                    return;
                }
            }
            // max 1024 << 9 = 524,288, enough to save
            wrap::path_string path;
            path.reserve(2400);
            path.resize(1024);
            for (size_t i = 0; i < 10; i++) {
                path.resize(path.size() << 1);
                auto red = readlink(proc, path.data(), path.size());
                if (red < path.size()) {
                    strutil::append(pb, path.c_str());
                    return;
                }
            }
            strutil::append(pb, path.c_str());
        }
#endif
    }  // namespace wrap
}  // namespace futils
