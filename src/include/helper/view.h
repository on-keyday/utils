/*license*/

// view - utility view
#pragma once
#include "../core/buffer.h"
#include "../endian/endian.h"

namespace utils {
    namespace helper {
        template <class T>
        struct ReverseView {
            Buffer<T> buf;

            using char_type = typename Buffer<T>::char_type;

            char_type operator[](size_t pos) const {
                return buf.at(pos);
            }

            size_t size() {
                return buf.size();
            }
        };

        template <class T, class Char>
        struct EndianView {
            Buffer<T> buf;
            bool reverse = false;

            using char_type = typename Buffer<T>::char_type;

            static_assert(sizeof(char_type) == 1, "expect sizeof(char_type) is 1");

            Char operator[](size_t pos) const {
                if (pos >= size()) {
                    return Char();
                }
                std::uint8_t tmp[sizeof(Char)] = {0};
                auto idx = pos * sizeof(Char);
                for (size_t i = 0; i < sizeof(Char); i++) {
                    tmp[i] = buf.at(idx + i);
                }
                if (reverse) {
                    return endian::internal::reverse_endian<Char>(tmp);
                }
                else {
                    return endian::internal::copy_as_is<Char>(tmp);
                }
            }

            size_t size() const {
                auto sz = buf.size();
                if (sz % sizeof(Char)) {
                    return 0;
                }
                return sz / sizeof(Char);
            }
        };
    }  // namespace helper
}  // namespace utils
