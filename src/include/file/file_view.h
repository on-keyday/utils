/*license*/

// file_view - file view
// need to link libutils
#pragma once
#include "platform.h"
#include "../wrap/lite/string.h"
#include "../utf/convert.h"

namespace utils {
    namespace file {
        class View {
            mutable platform::ReadFileInfo info;

           public:
            bool open(const wrap::path_char* path);

            template <class Tmpbuf = wrap::path_string, class String>
            bool open(String&& str) {
                Tmpbuf result;
                if (!utf::convert(str, result)) {
                    return false;
                }
                return open(static_cast<const wrap::path_char*>(result.c_str()));
            }

            bool is_open() const;

            size_t size() const;

            std::uint8_t operator[](size_t position) const;

            void close();
        };
    }  // namespace file
}  // namespace utils
