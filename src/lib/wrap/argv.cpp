/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/wrap/argv.h"
#ifdef _WIN32
#include <windows.h>
#endif

namespace utils {
    namespace wrap {
#ifdef _WIN32

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
}  // namespace utils
