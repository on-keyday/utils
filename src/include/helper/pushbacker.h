/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// pushbacker - push_back utility
#pragma once

#include <type_traits>
#include <cstddef>

namespace utils {
    namespace helper {
        struct NopPushBacker {
            constexpr NopPushBacker() {}
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

        struct IPushBacker {
           private:
            void* ptr;
            void (*push_back_)(void*, std::uint8_t);

            template <class T>
            static void push_back_fn(void* self, std::uint8_t c) {
                static_cast<T*>(self)->push_back(c);
            }

           public:
            IPushBacker(const IPushBacker& b)
                : ptr(b.ptr), push_back_(b.push_back_) {}

            template <class T>
            IPushBacker(T& pb)
                : ptr(std::addressof(pb)), push_back_(push_back_fn<T>) {}

            void push_back(std::uint8_t c) {
                push_back_(ptr, c);
            }
        };

    }  // namespace helper
}  // namespace utils