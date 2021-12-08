/*license*/

// strutil - string utility
#pragma once

#include "../core/sequencer.h"

namespace utils {
    namespace helper {
        template <class T, class U>
        using compare_type = typename Sequencer<typename BufferType<T&>::type>::compare_type<U>;

        template <class T, class U>
        constexpr auto default_compare() {
            return Sequencer<typename BufferType<T&>::type>::default_compare<U>();
        }

        template <class In, class Cmp, class Compare = compare_type<In, Cmp>>
        constexpr bool starts_with(In&& in, Cmp&& cmp, Compare compare = default_compare<In, Cmp>()) {
            Sequencer<typename BufferType<In&>::type> intmp(in);
            return intmp.match(cmp, compare);
        }
    }  // namespace helper
}  // namespace utils
