/*license*/

#include "platform.h"

namespace utils {
    namespace file {
        struct View {
            platform::ReadFileInfo info;
            size_t virtual_ptr = 0;
            bool open() {
                return info.open();
            }
        };
    }  // namespace file
}  // namespace utils