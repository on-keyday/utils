/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// matching - bnf rule matching
#pragma once
#include "bnf.h"

namespace utils {
    namespace bnf {
        struct StackFrame {
            std::shared_ptr<BnfTree> bnf;
            size_t rptr = 0;
            size_t counter = 0;
        };

        enum class StackState {
            next,
            continues,
            failure,
            fatal,
        };

        namespace internal {

            template <class Stack, class Seq, class ErrC, class Callback>
            struct Context {
                Stack& stack;
                Seq& seq;
                ErrC& errc;
                // cb.on_match(Stack stack,string rule,string matched,BnfType type,size_t repeat);
                // cb.on_stack(Stack stack,bool enter,StackState prev);
                Callback& cb;
                std::shared_ptr<BnfTree> cur;
            };

            std::pair<size_t, size_t> get_range(const Annotation& annon) {
                size_t min_ = 1, max_ = 1;
                if (annon.range.enable) {
                    min_ = annon.range.cache_min_;
                    max_ = annon.range.cache_max_;
                }
                else {
                    if (annon.optional) {
                        min_ = 0;
                    }
                    if (annon.repeat) {
                        max_ = ~0;
                    }
                }
                return {min_, max_};
            }

            template <class Stack, class Seq, class ErrC, class Callback>
            StackState match_literal(Context<Stack, Seq, ErrC, Callback>& ctx) {
                auto [min_, max_] = get_range(ctx.cur->annon);
                if (max_ == 0) {
                    ctx.errc.say("expect literal ", ctx.cur->node->str, " but range max is 0. is this bug?");
                    ctx.errc.node(ctx.cur->node);
                    return StackState::fatal;
                }
                size_t count = 0;
                while (true) {
                    if (!ctx.seq.seek_if(ctx.cur->fmtcache)) {
                        if (count < min_) {
                            ctx.errc.say("expect literal ", ctx.cur->node->str, " least ", min_, " but not");
                            ctx.errc.node(ctx.cur->node);
                            return ctx.cur->annon.enfatal ? StackState::fatal : StackState::failure;
                        }
                        ctx.cb.on_match(std::as_const(ctx.stack), std::as_const(ctx.cur->node->str), std::as_const(ctx.cur->fmtcache), BnfType::literal, count);
                        return StackState::next;
                    }
                    count++;
                    if (count >= max_) {
                        break;
                    }
                }
                ctx.cb.on_match(std::as_const(ctx.stack), std::as_const(ctx.cur->node->str), std::as_const(ctx.cur->fmtcache), BnfType::literal, count);
                return StackState::next;
            }

            template <class Stack, class Seq, class ErrC, class Callback>
            StackState match_charcode(Context<Stack, Seq, ErrC, Callback>& ctx) {
                auto [min_, max_] = get_range(ctx.cur->annon);
                if (max_ == 0) {
                    ctx.errc.say("expect literal ", ctx.cur->node->str, " but range max is 0. is this bug?");
                    ctx.errc.node(ctx.cur->node);
                    return StackState::fatal;
                }
                size_t count = 0;
                while (true) {
                    if (!ctx.seq.seek_if(ctx.cur->fmtcache)) {
                        if (count < min_) {
                            ctx.errc.say("expect charcode %", ctx.cur->node->str, " least ", min_, " but not");
                            ctx.errc.node(ctx.cur->node);
                            return ctx.cur->annon.enfatal ? StackState::fatal : StackState::failure;
                        }
                        ctx.cb.on_match(std::as_const(ctx.stack), std::as_const(ctx.cur->node->str), std::as_const(ctx.cur->fmtcache), BnfType::char_code, count);
                        return StackState::next;
                    }
                    count++;
                    if (count >= max_) {
                        break;
                    }
                }
                ctx.cb.on_match(std::as_const(ctx.stack), std::as_const(ctx.cur->node->str), std::as_const(ctx.cur->fmtcache), BnfType::char_code, count);
                return StackState::next;
            }

            template <class Stack, class Seq, class ErrC, class Callback>
            StackState match(Context<Stack, Seq, ErrC, Callback>& ctx) {
                if (ctx.cur->type == BnfType::group || ctx.cur->type == BnfType::or_) {
                    auto child = ctx.cur->child;
                    ctx.stack.push_back(StackFrame{ctx.cur, ctx.seq.rptr, 0});
                    ctx.cur = std::move(child);
                    ctx.cb.on_stack(std::as_const(ctx.stack), true, StackState::continues);
                    return StackState::continues;
                }
                else if (ctx.cur->type == BnfType::rule_ref) {
                    auto ref = ctx.cur->ref.lock();
                    if (!ref) {
                        if (ctx.cur->node->str == "EOF") {
                            if (!ctx.seq.eos()) {
                                ctx.errc.say("expect EOF but not");
                                ctx.errc.node(ctx.cur->node);
                                return ctx.cur->annon.enfatal ? StackState::fatal : StackState::failure;
                            }
                            return StackState::next;
                        }
                        ctx.errc.say("unresolved rule reference name :", ctx.cur->node->str);
                        ctx.errc.node(ctx.cur->node);
                        return StackState::fatal;
                    }
                    ctx.stack.push_back(StackFrame{ctx.cur, ctx.seq.rptr, 0});
                    ctx.cur = ref->bnf;
                    ctx.cb.on_stack(std::as_const(ctx.stack), true, StackState::continues);
                    return StackState::continues;
                }
                else if (ctx.cur->type == BnfType::literal) {
                    return match_literal(ctx);
                }
                else if (ctx.cur->type == BnfType::char_code) {
                    return match_charcode(ctx);
                }
                return StackState::failure;
            }

            template <class Stack, class Seq, class ErrC, class Callback>
            StackState stack_back_or(StackState prev, Context<Stack, Seq, ErrC, Callback>& ctx, StackFrame& top) {
                if (top.counter == 0) {
                    if (prev != StackState::failure) {
                        ctx.errc.clear();
                        ctx.cur = top.bnf->next;
                        ctx.cb.on_match(std::as_const(ctx.stack), std::as_const(top.bnf->node->str), std::string(), BnfType::or_, top.counter);
                        return StackState::continues;
                    }
                    top.counter = 1;
                    auto els = top.bnf->els;
                    ctx.seq.rptr = top.rptr;
                    ctx.stack.push_back(std::move(top));
                    ctx.cb.on_stack(std::as_const(ctx.stack), true, StackState::continues);
                    ctx.cur = std::move(els);
                    return StackState::continues;
                }
                else if (top.counter == 1) {
                    if (prev != StackState::failure) {
                        ctx.errc.clear();
                        ctx.cur = top.bnf->next;
                        ctx.cb.on_match(std::as_const(ctx.stack), std::as_const(top.bnf->node->str), std::string(), BnfType::or_, top.counter);
                        return StackState::continues;
                    }
                    ctx.errc.say("expect one of | element but not");
                    ctx.errc.node(top.bnf->node);
                    return top.bnf->annon.enfatal ? StackState::fatal : StackState::failure;
                }
                ctx.errc.say("unexpected stack frame. library bug!");
                ctx.errc.node(top.bnf->node);
                return StackState::fatal;
            }

            template <class Stack, class Seq, class ErrC, class Callback>
            StackState stack_back_repeatable(StackState prev, BnfType bnfty, Context<Stack, Seq, ErrC, Callback>& ctx, StackFrame& top, auto&& cb) {
                auto [min_, max_] = get_range(top.bnf->annon);
                if (prev == StackState::failure) {
                    if (top.counter < min_) {
                        ctx.errc.say("expect group least ", min_, " count but not");
                        ctx.errc.node(top.bnf->node);
                        return top.bnf->annon.enfatal ? StackState::fatal : StackState::failure;
                    }
                    ctx.errc.clear();
                    ctx.seq.rptr = top.rptr;
                    ctx.cur = top.bnf->next;
                    ctx.cb.on_match(std::as_const(ctx.stack), std::as_const(top.bnf->node->str), std::string(), bnfty, top.counter);
                    return StackState::continues;
                }
                top.counter++;
                if (top.counter >= max_) {
                    ctx.cur = top.bnf->next;
                    ctx.cb.on_match(std::as_const(ctx.stack), std::as_const(top.bnf->node->str), std::string(), bnfty, top.counter);
                    return StackState::continues;
                }
                if (bnfty == BnfType::rule_ref && ctx.seq.rptr == top.rptr) {
                    ctx.errc.say("detect infinite loop. rule reference with `*?` and inner rule also has `*?`?");
                    ctx.errc.node(top.bnf->node);
                    return StackState::fatal;
                }
                top.rptr = ctx.seq.rptr;
                if (!cb()) {
                    return StackState::fatal;
                }
                ctx.stack.push_back(std::move(top));
                ctx.cb.on_stack(std::as_const(ctx.stack), true, StackState::continues);
                return StackState::continues;
            }

            template <class Stack, class Seq, class ErrC, class Callback>
            StackState stack_back_group(StackState prev, Context<Stack, Seq, ErrC, Callback>& ctx, StackFrame& top) {
                return stack_back_repeatable(prev, BnfType::group, ctx, top, [&] {
                    ctx.cur = top.bnf->child;
                    return true;
                });
            }

            template <class Stack, class Seq, class ErrC, class Callback>
            StackState stack_back_rule_ref(StackState prev, Context<Stack, Seq, ErrC, Callback>& ctx, StackFrame& top) {
                return stack_back_repeatable(prev, BnfType::rule_ref, ctx, top, [&] {
                    auto ref = top.bnf->ref.lock();
                    if (!ref) {
                        ctx.errc.say("unresolved rule reference name :", ctx.cur->node->str);
                        ctx.errc.node(ctx.cur->node);
                        return false;
                    }
                    ctx.cur = ref->bnf;
                    return true;
                });
            }

            template <class Stack, class Seq, class ErrC, class Callback>
            StackState stack_back(StackState prev, Context<Stack, Seq, ErrC, Callback>& ctx) {
                ctx.cb.on_stack(std::as_const(ctx.stack), false, prev);
                StackFrame top = ctx.stack.back();  // invoker frame
                ctx.stack.pop_back();
                if (top.bnf->type == BnfType::or_) {
                    return stack_back_or(prev, ctx, top);
                }
                else if (top.bnf->type == BnfType::group) {
                    return stack_back_group(prev, ctx, top);
                }
                else if (top.bnf->type == BnfType::rule_ref) {
                    return stack_back_rule_ref(prev, ctx, top);
                }
                ctx.errc.say("unexpected stack_back call. library bug!");
                ctx.errc.node(top.bnf->node);
                return StackState::fatal;
            }

            template <class T, class Stack>
            concept has_on_match = requires(T t) {
                {t.on_match(std::declval<Stack>(), std::declval<const std::string&>(), std::declval<const std::string&>(), BnfType(), size_t())};
            };

            template <class T, class Stack>
            concept has_on_stack = requires(T t) {
                {t.on_stack(std::declval<Stack>(), bool(), StackState())};
            };
            template <class CB>
            struct CBWrap {
                CB& cb;
                void on_match(const auto& stack, auto&&... args) {
                    if constexpr (has_on_match<CB, decltype(stack)>) {
                        cb.on_match(stack, args...);
                    }
                    else if constexpr (std::invocable<CB, decltype(stack), const std::string&, const std::string&, BnfType(), size_t()>) {
                        cb(stack, args...);
                    }
                }

                void on_stack(const auto& stack, auto&&... args) {
                    if constexpr (has_on_stack<CB, decltype(stack)>) {
                        cb.on_stack(stack, args...);
                    }
                    else if constexpr (std::invocable<CB, decltype(stack), bool, StackState>) {
                        cb(stack, args...);
                    }
                }
            };
        }  // namespace internal

        // matching do matching seq to bnf rule
        // cb should be called as
        // cb.on_match(Stack stack,string rule,string matchstr,BnfTyoe type,size_t repeat)
        // cb.on_stack(Stack stack,bool enter,StackState prev)
        // or same signature cb() call
        bool matching(auto&& stack, const std::shared_ptr<BnfTree>& bnf, auto&& seq, auto&& errc, auto&& cb) {
            using stack_t = std::remove_reference_t<decltype(stack)>;
            using seq_t = std::remove_reference_t<decltype(seq)>;
            using errc_t = std::remove_reference_t<decltype(errc)>;
            using cb_t = std::remove_reference_t<decltype(cb)>;
            internal::CBWrap<cb_t> cbw{cb};
            internal::Context<stack_t, seq_t, errc_t, decltype(cbw)> ctx{stack, seq, errc, cbw, bnf};
            const auto base = ctx.stack.size();
            StackState state = StackState::failure;
            while (true) {
                while (ctx.cur) {
                    state = internal::match(ctx);
                    if (state == StackState::continues) {
                        continue;
                    }
                    if (state == StackState::failure) {
                        break;
                    }
                    if (state == StackState::fatal) {
                        return false;
                    }
                    ctx.cur = ctx.cur->next;
                }
                while (true) {
                    if (base == ctx.stack.size()) {
                        return state == StackState::failure ? false : true;
                    }
                    state = internal::stack_back(state, ctx);
                    if (state == StackState::failure) {
                        continue;
                    }
                    if (state == StackState::fatal) {
                        return false;
                    }
                    break;
                }
            }
        }
    }  // namespace bnf
}  // namespace utils
