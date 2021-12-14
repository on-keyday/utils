/*license*/

// view - utility view
#pragma once
#include "../core/buffer.h"

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
            bool revendian = false;

            using char_type = typename Buffer<T>::char_type;

            static_assert(sizeof(char_type) == 1, "expect sizeof(char_type) is 1");

            Char operator[](size_t sz) {
                if (buf.size() % sizeof(Char)) {
                    return Char();
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
