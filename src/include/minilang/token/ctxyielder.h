/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// ctxyielder - context yielder
#pragma once
#include "token.h"

namespace utils {
    namespace minilang::token {
        constexpr auto ctx_primitive() {
            return [](auto&& src) {
                return src.ctx.yielder.primitive(src);
            };
        }

        constexpr auto ctx_statement() {
            return [](auto&& src) {
                return src.ctx.yielder.statement(src);
            };
        }

        constexpr auto ctx_expr() {
            return [](auto&& src) {
                return src.ctx.yielder.expr(src);
            };
        }

        constexpr auto defined_primitive() {
            return [](auto&& src) -> std::shared_ptr<Token> {
                const auto trace = trace_log(src, "defined_primitive");
                if (!load_next(src)) {
                    error_log(src, "expect primitive value but unexpected eof");
                    return nullptr;
                }
                std::shared_ptr<Token>& next = src.next;
                if (next->kind != Kind::ident &&
                    next->kind != Kind::string &&
                    next->kind != Kind::number) {
                    error_log(src, "expect identifier or string literal or number but not");
                    return nullptr;
                }
                return std::move(next);
            };
        }
    }  // namespace minilang::token
}  // namespace utils
