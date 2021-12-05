/*license*/

#include "platform.h"
#include "../wrap/string.h"

namespace utils {
    namespace file {
        struct View {
            platform::ReadFileInfo info;
            size_t virtual_ptr = 0;
            bool open(const platform::path_char* path) {
                return info.open(path);
            }
        };
    }  // namespace file
}  // namespace utils