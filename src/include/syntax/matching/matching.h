/*license*/

// matching - do matching
#pragma once

#include "keyword_literal.h"
#include "group_ref_or.h"

namespace utils {
    namespace syntax {
        template <class String, template <class...> class Vec, template <class...> class Map>
        class Match {
           private:
            internal::MatcherHelper<String, Vec, Map> matcher;

            MatchState matching_loop() {
                internal::Stack<String, Vec>& stack = matcher.stack;
                while (!stack.end_group()) {
                    auto& v = *stack.current_vec();
                }
            }
        };
    }  // namespace syntax
}  // namespace utils
