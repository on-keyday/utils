/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../pos.h"
#include "../cbtype.h"
#include "../status.h"

namespace futils::comb2 {
    namespace ctxs {
#define HAS(method) template <class T, class... A>                 \
                    concept has_##method = requires(T t, A... a) { \
                        { t.method(a...) };                        \
                    };
#define CALL_IF(method, ...)                                                            \
    if constexpr (has_##method<decltype(ctx) __VA_OPT__(, decltype(__VA_ARGS__)...)>) { \
        ctx.method(__VA_ARGS__);                                                        \
    }

#define DEF(method, ...)                          \
    HAS(method)                                   \
    constexpr auto context_##method(auto&& ctx) { \
        CALL_IF(method);                          \
    }

        HAS(logic_entry);

        constexpr auto context_logic_entry(auto&& ctx, CallbackType type) {
            if constexpr (has_logic_entry<decltype(ctx), CallbackType>) {
                ctx.logic_entry(type);
            }
        }

        HAS(logic_result)

        constexpr auto context_logic_result(auto&& ctx, CallbackType type, Status status) {
            if constexpr (has_logic_result<decltype(ctx), CallbackType, Status>) {
                ctx.logic_result(type, status);
            }
        }

        HAS(error)
        HAS(error_seq)

        constexpr auto context_error(auto&& seq, auto&& ctx, auto&& err1, auto&&... arg) {
            if constexpr (has_error_seq<decltype(ctx), decltype(seq), decltype(err1), decltype(arg)...>) {
                ctx.error_seq(seq, err1, arg...);
            }
            else if constexpr (has_error<decltype(ctx), decltype(err1), decltype(arg)...>) {
                ctx.error(err1, arg...);
            }
        }

        HAS(end_string)

        constexpr auto context_end_string(auto&& ctx, Status res, auto&& tag, auto&& seq, Pos pos) {
            if constexpr (has_end_string<decltype(ctx), Status, decltype(tag), decltype(seq), Pos>) {
                ctx.end_string(res, tag, seq, pos);
            }
        }

        HAS(begin_string);

        constexpr auto context_begin_string(auto&& ctx, auto&& tag) {
            if constexpr (has_begin_string<decltype(ctx), decltype(tag)>) {
                ctx.begin_string(tag);
            }
        }

        HAS(begin_group)

        constexpr auto context_begin_group(auto&& ctx, auto&& tag) {
            if constexpr (has_begin_group<decltype(ctx), decltype(tag)>) {
                ctx.begin_group(tag);
            }
        }

        HAS(end_group)

        constexpr auto context_end_group(auto&& ctx, Status res, auto&& tag, Pos pos) {
            if constexpr (has_end_group<decltype(ctx), Status, decltype(tag), Pos>) {
                ctx.end_group(res, tag, pos);
            }
        }

        HAS(utf_error)

        constexpr auto context_is_fatal_utf_error(auto&& ctx, auto&& seq, auto err) {
            if constexpr (has_utf_error<decltype(ctx), decltype(seq), decltype(err)>) {
                return ctx.utf_error(seq, err);
            }
            else {
                return false;
            }
        }

        HAS(expect_indent)

        constexpr auto context_get_expect_indent(auto&& ctx) {
            if constexpr (has_expect_indent<decltype(ctx)>) {
                return ctx.expect_indent();
            }
            else {
                return -1;
            }
        }

        HAS(must_match_error)

        constexpr void context_call_must_match_error(auto&& seq, auto&& ctx, auto&& target, auto&& rec) {
            if constexpr (has_must_match_error<decltype(target), decltype(seq), decltype(ctx), decltype(rec)>) {
                target.must_match_error(seq, ctx, rec);
            }
        }

        template <class T>
        concept has_to_string = requires(T t) {
            { to_string(t) };
        };

    }  // namespace ctxs
#undef HAS
#undef CALL_IF
#undef DEF
}  // namespace futils::comb2
