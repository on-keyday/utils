/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// appender - append to other string
#pragma once

// #include "sfinae.h"
#include <core/sequencer.h>

namespace futils {
    namespace strutil {
        namespace internal {

            template <class T, class U>
            concept has_append = requires(T t, U u) {
                t.append(u);
            };

            constexpr void append_impl(auto& dst, const auto& src) {
                if constexpr (has_append<decltype(dst), decltype(src)>) {
                    dst.append(src);
                }
                else {
                    auto seq = make_ref_seq(src);
                    while (!seq.eos()) {
                        dst.push_back(seq.current());
                        seq.consume();
                    }
                }
            }
        }  // namespace internal

        template <class T, class U>
        constexpr void append(T& t, const U& u) {
            return internal::append_impl(t, u);
        }

        template <class T>
        constexpr void appends(T& t) {}

        template <class T, class First, class... Args>
        constexpr void appends(T& t, First&& f, Args&&... args) {
            append(t, f);
            appends(t, std::forward<Args>(args)...);
        }

        template <class T, class... Args>
        constexpr T concat(Args&&... args) {
            T t;
            appends(t, std::forward<Args>(args)...);
            return t;
        }
    }  // namespace strutil
}  // namespace futils
