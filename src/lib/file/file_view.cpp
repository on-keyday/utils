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
    }  // namespace file
}  // namespace utils
