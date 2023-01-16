/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// appender - append to other string
#pragma once

#include "sfinae.h"
#include "../core/sequencer.h"

namespace utils {
    namespace helper {
        namespace internal {
            /*
            SFINAE_BLOCK_TU_BEGIN(has_append, std::declval<T>().append(std::declval<const U>()))
            constexpr static void invoke(T& target, const U& to_append) {
                target.append(to_append);
            }
            SFINAE_BLOCK_TU_ELSE(has_append)
            constexpr static void invoke(T& target, const U& to_append) {
                auto seq = make_ref_seq(to_append);
                while (!seq.eos()) {
                    target.push_back(seq.current());
                    seq.consume();
                }
            }
            SFINAE_BLOCK_TU_END()
            */

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
    }  // namespace helper
}  // namespace utils
