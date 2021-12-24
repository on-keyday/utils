/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// pushbacker - push_back utility
#pragma once

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

    }  // namespace helper
}  // namespace utils