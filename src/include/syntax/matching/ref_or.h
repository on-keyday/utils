/*license*/

// ref_or - matching `or` element and `reference` element
#pragma once
#include "context.h"
#include "stack.h"
#include "state.h"

namespace utils {
    namespace syntax {
        namespace internal {
            template <class String, template <class...> class Vec>
            MatchState start_match_group(Vec<wrap::shared_ptr<Element<String, Vec>>>*& current, Context<String, Vec>& ctx, wrap::shared_ptr<Element<String, Vec>>& v, Stack<String, Vec>& stack) {
                assert(v);
                Group<String, Vec>* group = static_cast<Group<String, Vec>*>(std::addressof(*v));
                auto cr = ctx.r.from_current();
                stack.push(StackContext<String, Vec>{-1, v, std::move(ctx.r), current});
                ctx.r = std::move(cr);
                current = &group->group;
                return MatchState::succeed;
            }

            template <class String, template <class...> class Vec>
            MatchState start_match_or(Vec<wrap::shared_ptr<Element<String, Vec>>>*& current, Context<String, Vec>& ctx, wrap::shared_ptr<Element<String, Vec>>& v, Stack<String, Vec>& stack);

            template <class String, template <class...> class Vec>
            MatchState dispatch_under(Vec<wrap::shared_ptr<Element<String, Vec>>>*& current, Context<String, Vec>& ctx, wrap::shared_ptr<Element<String, Vec>>& v, Stack<String, Vec>& stack) {
                assert(v);
                Or<String, Vec>* or_ = static_cast<Or<String, Vec>*>(std::addressof(*v));
                assert(or_->or_list.size());
                auto list = or_->or_list[0];
                assert(list);
                if (list->type == SyntaxType::or_) {
                    return start_match_or(current, ctx, list, stack);
                }
                else if (list->type == SyntaxType::group) {
                    return start_match_group(current, ctx, list, stack);
                }
                else {
                    ctx.err << "fatal: unexpected Syntax type\n";
                    return MatchState::fatal;
                }
            }

            template <class String, template <class...> class Vec>
            MatchState start_match_or(Vec<wrap::shared_ptr<Element<String, Vec>>>*& current, Context<String, Vec>& ctx, wrap::shared_ptr<Element<String, Vec>>& v, Stack<String, Vec>& stack) {
                assert(v);
                auto cr = ctx.r.from_current();
                stack.push(StackContext<String, Vec>{0, v, std::move(ctx.r), current});
                ctx.r = std::move(cr);
                return dispatch_under(current, ctx, v, stack);
            }

            template <class String, template <class...> class Vec>
            MatchState result_match_or(MatchState prev, Vec<wrap::shared_ptr<Element<String, Vec>>>*& current, Context<String, Vec>& ctx, wrap::shared_ptr<Element<String, Vec>>& v, Stack<String, Vec>& stack) {
                StackContext<String, Vec> c = stack.pop();
                assert(c.element);
                Or<String, Vec>* or_ = static_cast<Or<String, Vec>*>(std::addressof(*c.element));
                auto cr = c.r;
                if (prev == MatchState::fatal) {
                    ctx.r = std::move(cr);
                    current = c.vec;
                    return MatchState::fatal;
                }
                else if (prev == MatchState::succeed) {
                    cr.seek_to(ctx.r);
                    ctx.r = std::move(cr);
                    if (any(or_->attr & Attribute::repeat)) {
                        c.on_repeat = true;
                        c.index = 0;
                        stack.push(std::move(c));
                        return dispatch_under(current, ctx, v, stack);
                    }
                    else {
                        current = c.vec;
                    }
                    return MatchState::succeed;
                }
                else {
                    c.index++;
                    if (or_->or_list.size() <= c.index) {
                        current = c.vec;
                        if (any(or_->attr & Attribute::fatal)) {
                            ctx.r = std::move(cr);
                            return MatchState::fatal;
                        }
                        else if (c.on_repeat || any(or_->attr & Attribute::ifexists)) {
                            cr.seek_to(ctx.r);
                            ctx.r = std::move(cr);
                            return MatchState::succeed;
                        }
                        return MatchState::not_match;
                    }
                    ctx.r = cr.from_current();
                    stack.push(std::move(c));
                    return MatchState::succeed;
                }
            }

            template <class String, template <class...> class Vec>
            MatchState result_match_group(MatchState prev, Vec<wrap::shared_ptr<Element<String, Vec>>>*& current, Context<String, Vec>& ctx, wrap::shared_ptr<Element<String, Vec>>& v, Stack<String, Vec>& stack) {
                StackContext<String, Vec> c = stack.pop();
                assert(c.element);
                Group<String, Vec>* group = static_cast<Group<String, Vec>*>(std::addressof(*c.element));
                auto cr = c.r;
                if (prev == MatchState::fatal) {
                    ctx.r = std::move(cr);
                    current = c.vec;
                    return MatchState::fatal;
                }
                else if (prev == MatchState::succeed) {
                    cr.seek_to(ctx.r);
                    ctx.r = std::move(cr);
                    if (any(group->attr & Attribute::repeat)) {
                        c.r = ctx.r;
                        ctx.r = c.r.from_current();
                        c.index = -1;
                        stack.push(std::move(c));
                    }
                    else {
                        current = c.vec;
                    }
                    return MatchState::succeed;
                }
                else {
                    current = c.vec;
                    if (any(group->attr & Attribute::fatal)) {
                        ctx.r = std::move(cr);
                        return MatchState::fatal;
                    }
                    else if (c.on_repeat || any(group->attr & Attribute::ifexists)) {
                        cr.seek_to(ctx.r);
                        ctx.r = std::move(cr);
                        return MatchState::succeed;
                    }
                    return MatchState::not_match;
                }
            }
        }  // namespace internal
    }      // namespace syntax
}  // namespace utils
