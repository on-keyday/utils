/*license*/

// matching - do matching
#pragma once

#include "keyword_literal.h"
#include "group_ref_or.h"
#include "context.h"

namespace utils {
    namespace syntax {
        template <class String, template <class...> class Vec, template <class...> class Map>
        class Match {
           private:
            internal::MatcherHelper<String, Vec, Map> matcher;

            MatchState matching_loop() {
                internal::Stack<String, Vec>& stack = matcher.stack;
                MatchState state = MatchState::not_match;
                while (!stack.end_group()) {
                    auto& current = stack.current();
                    wrap::shared_ptr<Element<String, Vec>> v = (*current.vec)[current.index];
                    if (v->type == SyntaxType::keyword) {
                        MatchResult<String> result;
                        state = internal::match_keyword(matcher.context, v, result);
                        if (state == MatchState::fatal) {
                            break;
                        }
                        else if (state == MatchState::succeed) {
                            if (any(current.attr & Attribute::repeat)) {
                                current.on_repeat = true;
                                continue;
                            }
                            current.index++;
                            continue;
                        }
                        else {
                            if (current.on_repeat || any(current.attr & Attribute::ifexists)) {
                                current.index++;
                                continue;
                            }
                            break;
                        }
                    }
                }
            }
        };
    }  // namespace syntax
}  // namespace utils
