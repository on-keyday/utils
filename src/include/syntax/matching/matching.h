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
                while (true) {
                    while (!stack.end_group()) {
                        auto& current = stack.current();
                        wrap::shared_ptr<Element<String, Vec>> v = (*current.vec)[current.index];
                        auto invoke_matching = [&](auto&& f) {
                            MatchResult<String> result;
                            state = f(matcher.context, v, result);
                            if (state == MatchState::fatal) {
                                return state;
                            }
                            else if (state == MatchState::succeed) {
                                if (any(current.attr & Attribute::repeat)) {
                                    current.on_repeat = true;
                                    return state;
                                }
                                current.index++;
                                return state;
                            }
                            else {
                                if (current.on_repeat || any(current.attr & Attribute::ifexists)) {
                                    current.index++;
                                    return MatchState::succeed;
                                }
                                return MatchState::not_match;
                            }
                        };
                        if (v->type == SyntaxType::keyword) {
                            auto s = invoke_matching(internal::match_keyword<String, Vec>);
                            if (s != MatchState::succeed) {
                                break;
                            }
                        }
                        else if (v->type == SyntaxType::literal) {
                            auto s = invoke_matching(internal::match_literal<String, Vec>);
                            if (s != MatchState::succeed) {
                                break;
                            }
                        }
                        else if (v->type == SyntaxType::or_) {
                            state = matcher.start_or(v);
                            if (state != MatchState::succeed) {
                                break;
                            }
                        }
                        else if (v->type == SyntaxType::group) {
                            state = matcher.start_group(v);
                            if (state != MatchState::succeed) {
                                break;
                            }
                        }
                        else if (v->type == SyntaxType::reference) {
                            state = matcher.start_ref(v);
                            if (state != MatchState::succeed) {
                                break;
                            }
                        }
                        else {
                            matcher.context.err.packln("error: unexpected SyntaxType");
                            state = MatchState::fatal;
                        }
                    }
                    while (stack.stack.size()) {
                        auto& c = stack.current();
                        assert(c.element);
                        if (c.element->type == SyntaxType::or_) {
                            state = matcher.result_or(state);
                        }
                        else if (c.element->type == SyntaxType::reference) {
                            state = matcher.result_ref(state);
                        }
                        else if (c.element->type = SyntaxType::group) {
                            state = matcher.result_group(state);
                        }
                        else {
                            matcher.context.err.packln("error: unexpected SyntaxType");
                            return MatchState::fatal;
                        }
                        if (state != MatchState::succeed) {
                            continue;
                        }
                        break;
                    }
                }
            }

            MatchState matching(const char* root = "ROOT") {
                if (!root) {
                    return MatchState::fatal;
                }
            }
        };
    }  // namespace syntax
}  // namespace utils
