/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../pos.h"

namespace utils::comb2 {
    namespace ctxs {
#define HAS(method) template <class T, class... A>                 \
                    concept has_##method = requires(T t, A... a) { \
                        { t.method(a...) };                        \
                    };
#define CALL_IF(method, ...)                                                            \
    if constexpr (has_##method<decltype(ctx) __VA_OPT__(, decltype(__VA_ARGS__)...)>) { \
        ctx.repeat();                                                                   \
    }

#define DEF(method, ...)                          \
    HAS(method)                                   \
    constexpr auto context_##method(auto&& ctx) { \
        CALL_IF(method);                          \
    }
        DEF(repeat);

        DEF(step);

        DEF(branch);

        DEF(optional);

        DEF(commit);

        DEF(rollback);

        HAS(error)

        constexpr auto context_error(auto&& ctx, auto&&... arg) {
            CALL_IF(error, arg);
        }

        DEF(peek);

        HAS(end_string)

        constexpr auto context_end_string(auto&& ctx, auto&& tag, auto&& seq, Pos pos) {
            if constexpr (has_end_string<decltype(ctx), decltype(tag), decltype(seq), Pos>) {
                ctx.end_string(tag, seq, pos);
            }
        }

        DEF(begin_string);

        HAS(group)

        constexpr auto context_group(auto&& ctx, auto&& tag) {
            if constexpr (has_group<decltype(ctx), decltype(tag)>) {
                ctx.group(tag);
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

        template <class T>
        concept impl_required =
            has_repeat<T> &&
            has_step<T> &&
            has_optional<T> &&
            has_branch<T> &&
            has_commit<T> &&
            has_rollback<T>;
    }  // namespace ctxs
#undef HAS
#undef CALL_IF
#undef DEF
}  // namespace utils::comb2
