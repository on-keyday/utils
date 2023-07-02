/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../basic/proxy.h"
#include "../internal/context_fn.h"

namespace utils::comb2::composite {
    enum class IndentMode {
        less = 0x1,
        equal = 0x2,
        less_equal = 0x3,
        more = 0x4,
        more_equal = 0x6,
    };

    constexpr auto make_indent(IndentMode mode) {
        return ops::proxy([=](auto&& seq, auto&& ctx, auto&& rec) {
            auto count = ctx.indent();
            auto i = 0;
            for (; i < count; i++) {
                if (seq.current() != ' ') {
                    if (mode == IndentMode::less || mode == IndentMode::less_equal) {
                        ctx.indent(i);
                        return Status::match;
                    }
                    return Status::not_match;
                }
                seq.consume();
            }
            if (mode == IndentMode::less) {
                return Status::not_match;
            }
            if (mode == IndentMode::more || mode == IndentMode::more_equal) {
                auto expect = ctxs::context_get_expect_indent(ctx);
                auto from = i;
                if (mode == IndentMode::more) {
                    if (seq.current() != ' ') {
                        return Status::not_match;
                    }
                }
                while (seq.current() == ' ' && (expect >= 0 && i - from != expect)) {
                    i++;
                    seq.consume();
                }
                if (expect >= 0 && i - from != expect) {
                    return Status::not_match;
                }
            }
            ctx.indent(i);
            return Status::match;
        });
    }

    constexpr auto indent = make_indent(IndentMode::equal);
    constexpr auto new_indent = make_indent(IndentMode::more);
    constexpr auto new_or_eq_indent = make_indent(IndentMode::more_equal);
    constexpr auto less_indent = make_indent(IndentMode::less);
    constexpr auto less_eq_indent = make_indent(IndentMode::less_equal);
}  // namespace utils::comb2::composite
