/*license*/

// strutil - string utility
#pragma once

#include "../core/sequencer.h"

#include "view.h"

namespace utils {
    namespace helper {
        template <class T, class U>
        using compare_type = typename Sequencer<typename BufferType<T&>::type>::template compare_type<U>;

        template <class T, class U>
        constexpr auto default_compare() {
            return Sequencer<typename BufferType<T&>::type>::template default_compare<U>();
        }

        template <class In, class Cmp, class Compare = compare_type<In, Cmp>>
        constexpr bool starts_with(In&& in, Cmp&& cmp, Compare&& compare = default_compare<In, Cmp>()) {
            Sequencer<typename BufferType<In&>::type> intmp(in);
            return intmp.match(cmp, compare);
        }

        template <class In, class Cmp, class Compare = compare_type<In, Cmp>>
        constexpr bool ends_with(In&& in, Cmp&& cmp, Compare&& compare = default_compare<In, Cmp>()) {
            using reverse_type = ReverseView<typename BufferType<In&>::type>;
            Sequencer<reverse_type> intmp(reverse_type{in});
            ReverseView<typename BufferType<Cmp&>::type> cmptmp{cmp};
            return intmp.match(cmptmp, compare);
        }

        template <class In, class Cmp, class Compare = compare_type<In, Cmp>>
        constexpr bool equal(In&& in, Cmp&& cmp, Compare&& compare = default_compare<In, Cmp>()) {
            Sequencer<typename BufferType<In&>::type> intmp(in);
            return intmp.seek_if(cmp, compare) && intmp.eos();
        }

    }  // namespace helper
}  // namespace utils
