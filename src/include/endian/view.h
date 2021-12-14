/*license*/

// view - endian view
#pragma once

#include "endian.h"
#include "../core/buffer.h"

namespace utils {
    namespace endian {
        template <class T, class Char>
        struct EndianView {
            Buffer<typename BufferType<T>::type> buf;
            bool reverse = false;

            using char_type = typename Buffer<T>::char_type;

            static_assert(sizeof(char_type) == 1, "expect sizeof(char_type) is 1");

            constexpr Char operator[](size_t pos) const {
                if (pos >= size()) {
                    return Char();
                }
                std::uint8_t tmp[sizeof(Char)] = {0};
                auto idx = pos * sizeof(Char);
                for (size_t i = 0; i < sizeof(Char); i++) {
                    tmp[i] = buf.at(idx + i);
                }
                if (reverse) {
                    return internal::reverse_endian<Char>(tmp);
                }
                else {
                    return internal::copy_as_is<Char>(tmp);
                }
            }

            constexpr size_t size() const {
                auto sz = buf.size();
                if (sz % sizeof(Char)) {
                    return 0;
                }
                return sz / sizeof(Char);
            }
        };

    }  // namespace endian
}  // namespace utils