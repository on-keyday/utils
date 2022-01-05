/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// matching - do matching
#pragma once

#include "keyword_literal.h"
#include "group_ref_or.h"
#include "context.h"

namespace utils {
    namespace syntax {

        template <class String, template <class...> class Vec, template <class...> class Map>
        struct Match {
            internal::MatcherHelper<String, Vec, Map> matcher;

           private:
            template <class F>
            MatchState matching_loop(F&& cb) {
                internal::Stack<String, Vec>& stack = matcher.stack;
                MatchState state = MatchState::not_match;
                while (true) {
                    while (!stack.end_group()) {
                        auto& current = stack.current();
                        wrap::shared_ptr<Element<String, Vec>> v = (*current.vec)[current.index];
                        auto invoke_matching = [&](auto&& f) {
                            MatchContext<String, Vec> tmpctx{matcher.virtual_stack};
                            state = f(matcher.context, v, tmpctx.result);
                            if (state == MatchState::succeed) {
                                state = static_cast<MatchState>(cb(tmpctx));
                            }
                            if (state == MatchState::succeed) {
                                if (any(v->attr & Attribute::repeat)) {
                                    current.on_repeat = true;
                                    return state;
                                }
                                current.index++;
                                return state;
                            }
                            else if (state == MatchState::fatal) {
                                return state;
                            }
                            else {
                                if (any(v->attr & Attribute::fatal)) {
                                    return MatchState::fatal;
                                }
                                else if (any(v->attr & Attribute::ifexists)) {
                                    current.on_repeat = false;
                                    current.index++;
                                    return MatchState::succeed;
                                }
                                return MatchState::not_match;
                            }
                        };
                        if (v->type == SyntaxType::keyword) {
                            state = invoke_matching(internal::match_keyword<String, Vec>);
                            if (state != MatchState::succeed) {
                                break;
                            }
                        }
                        else if (v->type == SyntaxType::literal) {
                            state = invoke_matching(internal::match_literal<String, Vec>);
                            if (state != MatchState::succeed) {
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
                            break;
                        }
                    }
                    while (stack.stack.size()) {
                        auto& c = stack.current();
                        size_t top = stack.top();
                        auto prev = state;
                        if (!c.element) {
                            matcher.load_r(c, true);
                            return state;
                        }
                        if (c.element->type == SyntaxType::or_) {
                            state = matcher.result_or(state);
                        }
                        else if (c.element->type == SyntaxType::reference) {
                            state = matcher.result_ref(state);
                        }
                        else if (c.element->type == SyntaxType::group) {
                            state = matcher.result_group(state);
                        }
                        else {
                            matcher.context.err.packln("error: unexpected SyntaxType");
                            return MatchState::fatal;
                        }
                        if (state != MatchState::succeed) {
                            continue;
                        }
                        if (auto& cu = stack.current(); cu.element && cu.element->type == SyntaxType::group) {
                            if (stack.top() + 1 == top) {
                                stack.current().index++;
                            }
                        }
                        break;
                    }
                }
            }

           public:
            template <class F>
            MatchState matching(F&& f, const String& root = String()) {
                String v = root;
                if (!root.size()) {
                    utf::convert("ROOT", v);
                }
                matcher.stack.stack.clear();
                if (auto e = matcher.start_ref(v); e != MatchState::succeed) {
                    return e;
                }
                return matching_loop(std::forward<F>(f));
            }
        };
    }  // namespace syntax
}  // namespace utils
