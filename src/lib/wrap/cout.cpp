#include "../../include/wrap/cout.h"
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#define _O_U8TEXT 0
#endif

namespace utils {
    namespace wrap {
        static bool initialized = false;

        int stdinmode = _O_U8TEXT;
        int stdoutmode = _O_U8TEXT;
        int stderrmode = _O_U8TEXT;
        bool sync_stdio = false;

        static bool is_std(ostream& out) {
            auto addr = std::addressof(out);
#ifdef _WIN32
            if (addr == std::addressof(std::wcout) || addr == std::addressof(std::wcerr) ||
                addr == std::addressof(std::wclog))
#else
            if (addr == std::addressof(std::cout) || addr == std::addressof(std::cerr) ||
                addr == std::addressof(std::clog))
#endif
            {
                return true;
            }
            return false;
        }

        static bool io_init(ostream& out) {
            if (is_std(out)) {
#ifdef _WIN32
                if (_setmode(_fileno(stdin), stdinmode) == -1) {
                    //err = "error:text input mode change failed";
                    return false;
                }
                if (_setmode(_fileno(stdout), stdoutmode) == -1) {
                    //err = "error:text output mode change failed\n";
                    return false;
                }
                if (_setmode(_fileno(stderr), stderrmode) == -1) {
                    //err = "error:text error mode change failed\n";
                    return false;
                }
#endif
                std::ios_base::sync_with_stdio(sync_stdio);
                initialized = true;
            }
            return true;
        }

        void UtfOut::write(const path_string& p) {
            if (!initialized) {
                io_init(this->out);
            }
            this->out << p;
        }
    }  // namespace wrap
}  // namespace utils
