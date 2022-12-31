/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// brackets - () [] . -> etc...
#pragma once
#include "token.h"
#include "tree.h"

namespace utils {
    namespace minilang::token {

        constexpr auto brackets(auto&& begin, auto&& inner, auto&& end) {
            return [=](auto&& src, std::shared_ptr<Token>& prev, size_t b) -> std::shared_ptr<Token> {
                const auto trace = trace_log(src, "brackets");
                std::shared_ptr<Token> tok = begin(src);
                if (!tok) {
                    return nullptr;
                }
                std::shared_ptr<Token> in = inner(src);
                if (!in) {
                    error_log(src, "expect bracket inner element");
                    token_log(src, tok);
                    src.err = true;
                    return nullptr;
                }
                std::shared_ptr<Token> fin = end(src);
                if (!fin) {
                    error_log(src, "expect brackets close");
                    token_log(src, tok);
                    token_log(src, in);
                    src.err = true;
                    return nullptr;
                }
                size_t e = src.seq.rptr;
                pass_log(src, "brackets");
                auto tri = std::make_shared_for_overwrite<Brackets>();
                tri->pos.begin = b;
                tri->pos.end = e;
                tri->callee = std::move(prev);
                tri->left = std::move(tok);
                tri->center = std::move(in);
                tri->right = std::move(fin);
                return tri;
            };
        }

        /*
        constexpr auto after(auto&& next, auto&& expect) {
            return [=](auto&& src) -> std::shared_ptr<Token> {
                const auto trace = trace_log(src, "after");
                size_t b = src.seq.rptr;
                std::shared_ptr<Token> tok = next(src);
                if (!tok) {
                    return nullptr;
                }
                src.err = false;
                for (std::shared_ptr<Token> symbol = expect(src); symbol; symbol = expect(src)) {
                    std::shared_ptr<Token> after = next(src);
                    if (!after) {
                        error_log(src,"expect right hand of operator");
                        token_log(src,symbol);
                        src.err = true;
                        return nullptr;
                    }
                    auto tri = std::make_shared_for_overwrite<BinTree>();
                    tri->pos.begin = b;
                    tri->pos.end = src.seq.rptr;
                    tri->right = std::move(tok);
                    tri->left = std::move(after);
                    tri->symbol = std::move(symbol);
                    tok = std::move(tri);
                }
                if (src.err) {
                    return nullptr;
                }
                return tok;
            }
        }*/
    }  // namespace minilang::token
}  // namespace utils
