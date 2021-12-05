/*license*/

#include "../../include/file/file_view.h"

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
            if (info.mapptr) {
                if (position >= info.stat.st_size) {
                    return 0;
                }
                return info.mapptr[position];
            }
        }
    }  // namespace file
}  // namespace utils
