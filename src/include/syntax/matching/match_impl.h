/*license*/

// match_impl - implementation of matching
#pragma once

#include "stack.h"
#include "state.h"
#include "context.h"

namespace utils {
    namespace syntax {
        namespace internal {
            template <class String, template <class...> class Vec, template <class...> class Map>
            struct MatcherWrap {
                using element_t = wrap::shared_ptr<Element<String, Vec>>;
                using vec_t = Vec<element_t>;
                Stack<String, Vec> stack;
                Context<String, Vec> context;

               private:
                template <class T>
                T* cast(auto& v) {
                    return static_cast<T*>(std::addressof(*v));
                }

                void store_r(StackContext<String, Vec>& c) {
                    c.r = std::move(context.r);
                    context.r = c.r.from_current();
                }

                void push(vec_t& vec, element_t& v) {
                    StackContext<String, Vec> c;
                    c.vec = &gr->group;
                    store_r(c);
                    c.element = v;
                    stack.push(std::move(c));
                }

                void load_r(StackContext<String, Vec>& c, bool seek = false) {
                    if (seek) {
                        c.r.seek_to(context.r);
                    }
                    context.r = std::move(c.r);
                }

                MatchState judge_by_attribute(Attribute attr, bool on_repeat) {
                    if (any(attr & Attribute::fatal)) {
                        return MatchState::fatal;
                    }
                    else if (on_repeat || any(attr & Attribute::ifexists)) {
                        return MatchState::succeed;
                    }
                    return MatchState::not_match;
                }

               public:
                MatchState start_group(element_t& v) {
                    assert(v && v->type == SyntaxType::group);
                    Group<String, Vec>* gr = cast<Group<String, Vec>>(v);
                    push(gr->group, v);
                    return MatchState::succeed;
                }

                MatchState result_group(MatchState prev) {
                    StackContext<String, Vec> c = stack.pop();
                    if (prev == MatchState::fatal) {
                        load_r(c);
                        return MatchState::fatal;
                    }
                    else if (prev == MatchState::succeed) {
                        load_r(c, true);
                        if (any(c.element->attr & Attribute::repeat)) {
                            c.index = 0;
                            c.on_repeat = true;
                            store_r(c);
                            stack.push(std::move(c));
                        }
                        return MatchState::succeed;
                    }
                }
            };
        }  // namespace internal
    }      // namespace syntax
}  // namespace utils
