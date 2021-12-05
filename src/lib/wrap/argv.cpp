/*license*/

#include "../../include/wrap/argv.h"
#include <windows.h>

namespace utils {
    namespace wrap {
#ifdef _WIN32

        static bool get_warg(int& wargc, wchar_t**& wargv) {
            return (bool)(wargv = CommandLineToArgvW(GetCommandLineW(), &wargc));
        }
        U8Arg::U8Arg(int& argc, char**& argv)
            : argcvalue(argc), argvvalue(argv), argcplace(&argc), argvplace(&argv) {
            int wargc;
            wchar_t** wargv;
            auto result = get_warg(wargc, wargv);
            assert(result);
            replaced.translate(wargv, wargv + wargc);
            replaced.arg(argc, argv);
        }

        U8Arg::~U8Arg() {
            *argcplace = argcvalue;
            *argvplace = argvvalue;
        }
#endif
    }  // namespace wrap
}  // namespace utils
