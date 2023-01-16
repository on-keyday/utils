/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// block - block statement
#pragma once
#include "token.h"
#include "tokendef.h"
#include <vector>
#include "space.h"

namespace utils {
    namespace minilang::token {

        constexpr auto yield_eof() {
            return [](auto&& src) -> std::shared_ptr<Eof> {
                if (src.seq.eos()) {
                    auto eof = std::make_shared_for_overwrite<Eof>();
                    eof->pos.begin = src.seq.rptr;
                    eof->pos.end = src.seq.rptr;
                    return eof;
                }
                return nullptr;
            };
        }

        constexpr auto yield_bof() {
            return [](auto&& src) -> std::shared_ptr<Bof> {
                auto bof = std::make_shared_for_overwrite<Bof>();
                bof->pos.begin = src.seq.rptr;
                bof->pos.end = src.seq.rptr;
                return bof;
            };
        }

        template <template <class...> class Vec = std::vector>
        constexpr auto yield_block(auto&& begin, auto&& inner, auto&& end) {
            return [=](auto&& src) -> std::shared_ptr<Block<Vec>> {
                const auto trace = trace_log(src, "block");
                size_t b = src.seq.rptr;
                std::shared_ptr<Token> tok = begin(src);
                if (!tok) {
                    return nullptr;
                }
                Vec<std::shared_ptr<Token>> elements;
                for (;;) {
                    if (std::shared_ptr<Token> fin = end(src)) {
                        auto block = std::make_shared_for_overwrite<Block<Vec>>();
                        block->pos.begin = b;
                        block->pos.end = src.seq.rptr;
                        block->begin = std::move(tok);
                        block->elements = std::move(elements);
                        block->end = std::move(fin);
                        return block;
                    }
                    if (src.seq.eos()) {
                        break;
                    }
                    std::shared_ptr<Token> in = inner(src);
                    if (!in) {
                        error_log(src, "expect block inner element");
                        token_log(src, tok);
                        src.err = true;
                        return nullptr;
                    }
                    elements.push_back(std::move(in));
                }
                error_log(src, "unexpected eof at block");
                token_log(src, tok);
                src.err = true;
                return nullptr;
            };
        }

        template <template <class...> class Vec = std::vector, class Inner, class Skip = decltype(skip_spaces({}))>
        constexpr auto yield_until_eof(Inner&& inner, Skip&& skip = skip_spaces({})) {
            return yield_block(yield_bof(), inner, skip(yield_eof()));
        }
    }  // namespace minilang::token
}  // namespace utils
