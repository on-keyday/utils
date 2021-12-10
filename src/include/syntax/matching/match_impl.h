/*license*/

// match_impl - implementation of matching
#pragma once

#include "stack.h"
#include "state.h"
#include "context.h"

namespace utils {
    namespace syntax {
        namespace internal {

            // to coding easily, using class
            template <class String, template <class...> class Vec, template <class...> class Map>
            struct MatcherWrap {
                using element_t = wrap::shared_ptr<Element<String, Vec>>;
                using vec_t = Vec<element_t>;
                Stack<String, Vec> stack;
                Context<String, Vec> context;
                Map<String, element_t> segments;

               private:
                template <class T>
                T* cast(auto& v) {
                    return static_cast<T*>(std::addressof(*v));
                }

                void store_r(StackContext<String, Vec>& c) {
                    c.r = std::move(context.r);
                    context.r = c.r.from_current();
                }

                void push(vec_t* vec, element_t& v) {
                    StackContext<String, Vec> c;
                    c.vec = vec;
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

                MatchState init_or(element_t& v, size_t index) {
                    assert(v && v->type == SyntaxType::or_);
                    Or<String, Vec>* or_ = cast<Group<String, Vec>>(v);
                    assert(or_->or_list);
                    auto list = or_->or_list[index];
                    if (list->type == SyntaxType::or_) {
                        return start_or(v);
                    }
                    else if (list->type == SyntaxType::group) {
                        return start_group(v);
                    }
                    else {
                        context.err.packln("error: unexpected SyntaxType");
                        return MatchState::fatal;
                    }
                }

               public:
                MatchState start_group(element_t& v) {
                    assert(v && v->type == SyntaxType::group);
                    Group<String, Vec>* gr = cast<Group<String, Vec>>(v);
                    push(&gr->group, v);
                    return MatchState::succeed;
                }

                MatchState result_group(MatchState prev) {
                    StackContext<String, Vec> c = stack.pop();
                    if (prev == MatchState::fatal) {
                        load_r(c);
                        return MatchState::fatal;
                    }
                    else if (prev == MatchState::succeed) {
                        context.err.clear();
                        load_r(c, true);
                        if (any(c.element->attr & Attribute::repeat)) {
                            c.index = 0;
                            c.on_repeat = true;
                            store_r(c);
                            stack.push(std::move(c));
                        }
                        return MatchState::succeed;
                    }
                    else {
                        MatchState res = judge_by_attribute(c.element->attr, c.on_repeat);
                        load_r(c, res == MatchState::succeed);
                        return res;
                    }
                }

                MatchState start_or(element_t& v) {
                    push(nullptr, v);
                    return init_or(v, 0);
                }

                MatchState result_or(MatchState prev) {
                    StackContext<String, Vec> c = stack.pop();
                    if (prev == MatchState::fatal) {
                        load_r(c);
                        return MatchState::fatal;
                    }
                    else if (prev == MatchState::succeed) {
                        context.err.clear();
                        load_r(c, true);
                        if (any(c.element->attr & Attribute::repeat)) {
                            c.index = 0;
                            c.on_repeat = true;
                            store_r(c);
                            auto elm = c.element;
                            stack.push(std::move(c));
                            return init_or(elm, 0);
                        }
                        return MatchState::succeed;
                    }
                    else {
                        assert(c.element && c.element->type == SyntaxType::or_);
                        Or<String, Vec>* or_ = cast<Group<String, Vec>>(v);
                        if (c.index >= or_->or_list.size()) {
                            MatchState res = judge_by_attribute(or_->attr, c.on_repeat);
                            load_r(c, res == MatchState::succeed);
                            return res;
                        }
                        load_r(c);
                        c.index++;
                        store_r(c);
                        auto elm = c.element;
                        stack.push(std::move(c));
                        return init_or(elm, c.index);
                    }
                }

                MatchState start_ref(element_t& v) {
                    assert(v && v->type == SyntaxType::reference);
                    Single<String, Vec>* ref = cast<Single<String, Vec>>(v);
                    ref->tok->to_string();
                }
            };
        }  // namespace internal
    }      // namespace syntax
}  // namespace utils
