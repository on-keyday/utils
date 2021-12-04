/*license*/

// minibuffer - mimimum buffer for conversion
#pragma once

#include <cstdint>
#include "../core/buffer.h"

namespace utils {
    namespace utf {
        template <class C, size_t size = sizeof(C)>
        struct Minibuffer;

        template <class C>
        struct Minibuffer<C, 1> {
            C buf[4] = {0};
            size_t pos = 0;

            constexpr Minibuffer() {}

            template <class T>
            constexpr Minibuffer(T&& t) {
                Buffer<typename BufferType<T&>::type> buf(t);
                for (auto i = 0; i < buf.size(); i++) {
                    push_back(buf.at(i));
                }
            }

            constexpr void push_back(C c) {
                if (pos >= 4) {
                    return;
                }
                buf[pos] = c;
                pos++;
            }

            constexpr C operator[](size_t position) const {
                if (pos <= position) {
                    return C();
                }
                return buf[position];
            }

            constexpr size_t size() const {
                return pos;
            }

            constexpr void clear() const {
                buf[0] = 0;
                buf[1] = 0;
                buf[2] = 0;
                buf[3] = 0;
            }
        };

        template <class C>
        struct Minibuffer<C, 2> {
            C buf[2] = {0};
            size_t pos = 0;

            constexpr Minibuffer() {}

            template <class T>
            constexpr Minibuffer(T&& t) {
                Buffer<typename BufferType<T&>::type> buf(t);
                for (auto i = 0; i < buf.size(); i++) {
                    push_back(buf.at(i));
                }
            }

            constexpr void push_back(C c) {
                if (pos >= 2) {
                    return;
                }
                buf[pos] = c;
                pos++;
            }

            constexpr C operator[](size_t position) const {
                if (pos <= position) {
                    return C();
                }
                return buf[position];
            }

            constexpr size_t size() const {
                return pos;
            }

            constexpr void clear() const {
                buf[0] = 0;
                buf[1] = 0;
            }
        };

        template <class C>
        struct Minibuffer<C, 4> {
            C buf[1] = {0};
            size_t pos = 0;

            constexpr Minibuffer() {}

            template <class T>
            constexpr Minibuffer(T&& t) {
                Buffer<typename BufferType<T&>::type> buf(t);
                for (auto i = 0; i < buf.size(); i++) {
                    push_back(buf.at(i));
                }
            }

            constexpr void push_back(C c) {
                if (pos >= 1) {
                    return;
                }
                buf[pos] = c;
                pos++;
            }

            constexpr C operator[](size_t position) const {
                if (pos <= position) {
                    return C();
                }
                return buf[position];
            }

            constexpr size_t size() const {
                return pos;
            }

            constexpr void clear() const {
                buf[0] = 0;
            }
        };
    }  // namespace utf
}  // namespace utils
