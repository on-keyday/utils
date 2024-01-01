/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/windows/dllexport_source.h>
#include <wrap/argv.h>
#include <platform/detect.h>
#ifdef FUTILS_PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace futils {
    namespace wrap {
#ifdef FUTILS_PLATFORM_WINDOWS

        static bool get_warg(int* wargc, wchar_t*** wargv) {
            return static_cast<bool>((*wargv = ::CommandLineToArgvW(::GetCommandLineW(), wargc)));
        }
        U8Arg::U8Arg(int& argc, char**& argv)
            : argcvalue(argc), argvvalue(argv), argcplace(&argc), argvplace(&argv) {
            int wargc;
            wchar_t** wargv;
            auto result = get_warg(&wargc, &wargv);
            assert(result);
            replaced.translate(wargv, wargv + wargc);
            ::LocalFree(wargv);
            replaced.arg(argc, argv);
        }

        U8Arg::~U8Arg() {
            *argcplace = argcvalue;
            *argvplace = argvvalue;
        }
#endif
    }  // namespace wrap
}  // namespace futils
