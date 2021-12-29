/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// view - utility view
#pragma once
#include "../core/buffer.h"

namespace utils {
    namespace helper {
        template <class T>
        struct ReverseView {
            Buffer<buffer_t<T>> buf;

            template <class... In>
            constexpr ReverseView(In&&... in)
                : buf(std::forward<In>(in)...) {}

            using char_type = typename Buffer<buffer_t<T>>::char_type;

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
            T c[2];
            size_t size_;

            constexpr CharView(T t, size_t sz = 1)
                : c{t, 0}, size_(sz) {}

            constexpr T operator[](size_t pos) const {
                return c[0];
            }

            constexpr size_t size() const {
                return size_;
            }

            constexpr const T* c_str() const {
                return c;
            }
        };

        template <class T>
        struct Slice {
            Buffer<buffer_t<T>> buf;
            size_t start = 0;
            size_t end = 0;
            size_t stride = 1;

            template <class V>
            constexpr Slice(V&& t)
                : buf(std::forward<V>(t)) {}

            using char_type = std::remove_cvref_t<typename BufferType<T>::char_type>;

            constexpr char_type operator[](size_t index) {
                size_t ofs = stride <= 1 ? 1 : stride;
                auto sz = buf.size();
                auto idx = ofs * index;
                auto ac = start + idx;
                if (ac >= end || ac >= buf.size()) {
                    return char_type{};
                }
                return buf.at(ac);
            }

            constexpr size_t size() const {
                size_t d = stride <= 1 ? 1 : stride;
                return (end - start) / d;
            }
        };

        template <class T>
        constexpr Slice<buffer_t<T&>> make_ref_slice(T&& in, size_t start, size_t end, size_t stride = 1) {
            Slice<buffer_t<T&>> slice{in};
            slice.start = start;
            slice.end = end;
            slice.stride = stride;
            return slice;
        }

    }  // namespace helper
}  // namespace utils
