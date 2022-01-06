/*license*/

// appender - append to other string
#pragma once

#include "sfinae.h"
#include "../core/sequencer.h"

namespace utils {
    namespace helper {
        namespace internal {
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
        }  // namespace internal

        template <class T, class U>
        constexpr void append(T& t, const U& u) {
            return internal::has_append<T, U>::invoke(t, u);
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
