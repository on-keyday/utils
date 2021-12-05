/*license*/

#include "platform.h"
#include "../wrap/string.h"
#include "../utf/convert.h"

namespace utils {
    namespace file {
        class View {
            mutable platform::ReadFileInfo info;
            mutable size_t virtual_ptr = 0;

           public:
            bool open(const platform::path_char* path);

            template <class String, class Tmpbuf = wrap::path_string>
            bool open(String&& str) {
                Tmpbuf result;
                if (!utf::convert(str, result)) {
                    return false;
                }
                return open(static_cast<const platform::path_char*>(result.c_str()));
            }

            size_t size() const;

            std::uint8_t operator[](size_t position) const;
        };
    }  // namespace file
}  // namespace utils
