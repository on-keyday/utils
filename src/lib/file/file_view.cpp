/*license*/

#include "../../include/file/file_view.h"

#ifdef _WIN32
#define _fseek(file, pos, origin) ::_fseeki64(file, static_cast<long long>(pos), origin)
#else
#define _fseek(file, pos, origin) ::fseek(file, pos, origin)
#endif

namespace utils {
    namespace file {
        bool View::open(const platform::path_char* path) {
            return this->info.open(path);
        }

        size_t View::size() const {
            return this->info.stat.st_size;
        }

        void View::close() {
            this->info.close();
            this->virtual_ptr = 0;
        }

        std::uint8_t View::operator[](size_t position) const {
            if (position >= info.stat.st_size) {
                return 0;
            }
            if (info.mapptr) {
                return info.mapptr[position];
            }
            else if (info.file) {
                _fseek(info.file, position, SEEK_SET);
            }
            return 0;
        }
    }  // namespace file
}  // namespace utils
