/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// test - test util
#pragma once
#include <cstddef>

namespace futils {
    namespace fnet::quic::test {
        template <class T>
        struct FixedTestVec {
            T buf[3]{};
            size_t index = 0;

            constexpr void push_back(T input) noexcept {
                if (index == 3) {
                    return;
                }
                buf[index] = input;
                index++;
            }

            constexpr const T* begin() const noexcept {
                return buf;
            }

            constexpr const T* end() const noexcept {
                return buf + index;
            }

            constexpr decltype(auto) operator[](size_t i) const noexcept {
                return buf[i];
            }

            constexpr void clear() {
                index = 0;
            }

            constexpr size_t size() const {
                return index;
            }
        };

    }  // namespace fnet::quic::test
}  // namespace futils
