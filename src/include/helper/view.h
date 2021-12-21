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

        struct NopPushBacker {
            template <class T>
            constexpr void push_back(T&&) const {}
        };

        constexpr NopPushBacker nop = {};

        template <class Buf = NopPushBacker>
        struct CountPushBacker {
            Buf buf;
            size_t count = 0;
            template <class T>
            constexpr void push_back(T&& t) {
                count++;
                buf.push_back(std::forward<T>(t));
            }
        };

        template <class Buf, size_t size_>
        struct FixedPushBacker {
            Buf buffer;
            size_t count;
            template <class T>
            constexpr void push_back(T&& t) {
                if (count >= size_) {
                    return;
                }
                buffer[count] = t;
            }

            constexpr size_t size() const {
                return size_;
            }
        };

    }  // namespace helper
}  // namespace utils
