/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// tokenizer - implementation of tokenizer
#pragma once

#include "token.h"
#include "line.h"
#include "space.h"
#include "predefined.h"
#include "identifier.h"
#include "comment.h"

namespace utils {
    namespace tokenize {

        template <class String, template <class...> class Vec>
        struct Tokenizer {
            Predefined<String, Vec> keyword;
            Predefined<String, Vec> symbol;
            PredefinedCtx<String, Vec> context_keyword;

            using token_t = wrap::shared_ptr<Token<String>>;

            template <class T>
            bool tokenize(Sequencer<T>& input, token_t& output) {
                output = wrap::make_shared<Token<String>>();
                auto current = output;
                auto to_next = [&](auto& tmp) {
                    current->next = tmp;
                    tmp->prev = std::addressof(*current);
                    current = tmp;
                };
                while (!input.eos()) {
                    {
                        size_t count = 0;
                        LineKind kind = LineKind::unknown;
                        if (read_line(input, kind, count)) {
                            auto tmp = wrap::make_shared<Line<String>>();
                            tmp->line = kind;
                            tmp->count = count;
                            to_next(tmp);
                            continue;
                        }
                    }
                    {
                        size_t count = 0;
                        char16_t space = 0;
                        if (read_space(input, space, count)) {
                            auto tmp = wrap::make_shared<Space<String>>();
                            tmp->space = space;
                            tmp->count = count;
                            to_next(tmp);
                            continue;
                        }
                    }
                    {
                        String token;
                        if (read_predefined<false>(input, symbol, token)) {
                            auto tmp = wrap::make_shared<Predef<String>>(TokenKind::symbol);
                            tmp->token = std::move(token);
                            to_next(tmp);
                            continue;
                        }
                    }
                    {
                        String token;
                        if (read_predefined<true>(input, keyword, token, symbol)) {
                            auto tmp = wrap::make_shared<Predef<String>>(TokenKind::keyword);
                            tmp->token = std::move(token);
                            to_next(tmp);
                            continue;
                        }
                    }
                    {
                        String token;
                        size_t layer;
                        if (read_predefined_ctx<true>(input, context_keyword, token, layer, symbol)) {
                            auto tmp = wrap::make_shared<PredefCtx<String>>();
                            tmp->token = std::move(token);
                            tmp->layer_ = layer;
                            to_next(tmp);
                            continue;
                        }
                    }
                    {
                        String identifier;
                        if (read_identifier(input, identifier, symbol)) {
                            auto tmp = wrap::make_shared<Identifier<String>>();
                            tmp->identifier = std::move(identifier);
                            to_next(tmp);
                            continue;
                        }
                    }
                    return false;
                }
                return true;
            }
        };
    }  // namespace tokenize
}  // namespace utils
