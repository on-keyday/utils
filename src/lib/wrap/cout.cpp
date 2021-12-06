#include "../../include/wrap/cout.h"
#include <cstdio>
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

        static ::FILE* is_std(ostream& out) {
            auto addr = std::addressof(out);
#ifdef _WIN32
            if (addr == std::addressof(std::wcout))
#else
            if (addr == std::addressof(std::cout))
#endif
            {
                return stdout;
            }
#ifdef _WIN32
            else if (addr == std::addressof(std::wcerr) ||
                     addr == std::addressof(std::wclog))
#else
            else if (addr == std::addressof(std::cerr) ||
                     addr == std::addressof(std::clog))
#endif
            {
                return stderr;
            }
            return nullptr;
        }

        static bool io_init() {
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
            return true;
        }

        UtfOut::UtfOut(ostream& out)
            : out(out) {
            std_handle = is_std(this->out);
        }

        void UtfOut::write(const path_string& p) {
            if (std_handle) {
                if (!initialized) {
                    auto result = io_init();
                    assert(result && "io init failed");
                }
                ::fwrite(p.c_str(), sizeof(path_char), p.size(), std_handle);
                ::fflush(std_handle);
            }
            else {
                this->out << p;
            }
        }

        UtfOut& UtfOut::operator<<(internal::Pack&& pack) {
            write(pack.impl.result);
            return *this;
        }

        UtfOut& cout_wrap() {
#ifdef _WIN32
            static UtfOut cout(std::wcout);
#else
            static UtfOut cout(std::cout);
#endif
            return cout;
        }

        UtfOut& cerr_wrap() {
#ifdef _WIN32
            static UtfOut cerr(std::wcerr);
#else
            static UtfOut cerr(std::cerr);
#endif
            return cerr;
        }
    }  // namespace wrap
}  // namespace utils
