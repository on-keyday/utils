/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// pushbacker - push_back utility
#pragma once

#include <type_traits>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <memory>

namespace utils {
    namespace helper {
        struct NopPushBacker {
            constexpr NopPushBacker() {}
            template <class T>
            constexpr void push_back(T&&) const noexcept {}
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
            Buf buf{};
            size_t count = 0;

            using char_type = std::remove_cvref_t<decltype(buf[0])>;

            constexpr FixedPushBacker() {}

            template <class T>
            constexpr void push_back(T&& t) {
                if (count >= size_) {
                    return;
                }
                buf[count] = t;
                count++;
            }

            constexpr size_t size() const {
                return count;
            }
        };

        template <class C = std::uint8_t>
        struct IPushBacker {
           private:
            void* ptr;
            void (*push_back_)(void*, C);

            template <class T>
            static void push_back_fn(void* self, C c) {
                static_cast<T*>(self)->push_back(c);
            }

           public:
            using char_type = C;

            IPushBacker(const IPushBacker& b)
                : ptr(b.ptr), push_back_(b.push_back_) {}

            template <class T>
            IPushBacker(T& pb)
                : ptr(std::addressof(pb)), push_back_(push_back_fn<T>) {}

            constexpr void push_back(C c) {
                push_back_(ptr, c);
            }
        };

        template <class Char>
        struct CharVecPushbacker {
            Char* text = nullptr;
            size_t size_ = 0;
            size_t cap = 0;
            bool overflow = false;

            constexpr bool full() const {
                return size_ >= cap;
            }
            constexpr void push_back(Char c) {
                if (full()) {
                    overflow = true;
                    return;
                }
                text[size_] = c;
                size_++;
            }

            constexpr size_t size() const {
                return size_;
            }

            constexpr const Char& operator[](size_t i) const {
                return text[i];
            }

            constexpr CharVecPushbacker(Char* t, size_t cap)
                : text(t), size_(0), cap(cap) {}

            constexpr CharVecPushbacker(Char* t, size_t size, size_t cap)
                : text(t), size_(size), cap(cap) {}
        };
    }  // namespace helper
}  // namespace utils
