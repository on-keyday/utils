#include "../../include/wrap/cout.h"
#include <iostream>

namespace utils {
    namespace wrap {
        static bool initialized = false;

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

        static void io_init(ostream& out) {
            if (is_std(out)) {
                initialized = true;
            }
        }

        void UtfOut::write(const path_string& p) {
            if (!initialized) {
                io_init(this->out);
            }
            this->out << p;
        }
    }  // namespace wrap
}  // namespace utils
