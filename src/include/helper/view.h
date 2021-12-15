/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// view - utility view
#pragma once
#include "../core/buffer.h"
#include "../endian/endian.h"

namespace utils {
    namespace helper {
        template <class T>
        struct ReverseView {
            Buffer<buffer_t<T>> buf;

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

            constexpr size_t size() const {
                return size_;
            }
        };

        template <class T>
        struct CharView {
            T c;
            size_t size_;

            constexpr CharView(T t, size_t sz = 1)
                : c(t), size_(sz) {}

            constexpr T operator[](size_t pos) const {
                return c;
            }

            constexpr size_t size() const {
                return size_;
            }
        };
    }  // namespace helper
}  // namespace utils
