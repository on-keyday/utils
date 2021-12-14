/*license*/

// view - utility view
#pragma once
#include "../core/buffer.h"
#include "../endian/endian.h"

namespace utils {
    namespace helper {
        template <class T>
        struct ReverseView {
            Buffer<typename BufferType<T>::type> buf;

            template <class... In>
            constexpr ReverseView(In&&... in)
                : buf(std::forward<In>(in)...) {}

            using char_type = typename Buffer<T>::char_type;

            constexpr char_type operator[](size_t pos) const {
                return buf.at(buf.size() - pos - 1);
            }

            constexpr size_t size() const {
                return buf.size();
            }
        };

        template <class T>
        struct SizedView {
            T* ptr;
            size_t size_;
            constexpr SizedView(T* t, size_t sz)
                : ptr(t), size_(sz) {}

            constexpr T& operator[](size_t pos) const {
                return ptr[pos];
            }

            size_t size() const {
                return size_;
            }
        };
    }  // namespace helper
}  // namespace utils
