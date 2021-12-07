/*license*/

// tokenizer - implementation of tokenizer
#pragma once

#include "token.h"
#include "line.h"
#include "space.h"
#include "predefined.h"

namespace utils {
    namespace tokenize {

        template <class String, template <class...> class Vec>
        struct Tokenizer {
            Predefined<String, Vec> keyword;
            Predefined<String, Vec> symbol;
            PredefinedCtx<String, Vec> context_keyword;

            template <class T>
            bool tokenize(Sequencer<T>& input, wrap::shared_ptr<Token<String>>& output) {
                output = wrap::make_shared<Token<String>>();
                auto current = output;
                auto to_next = [&](auto& tmp) {
                    current->next = tmp;
                    current = tmp;
                };
                while (!input.eos()) {
                    {
                        size_t count = 0;
                        LineKind kind = LineKind::unknown;
                        if (read_line(input, kind, count)) {
                            auto tmp = wrap::make_shared<Line<String>>();
                            tmp->kind = TokenKind::line;
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
                            tmp->kind = TokenKind::space;
                            tmp->space = space;
                            tmp->count = count;
                            to_next(tmp);
                            continue;
                        }
                    }
                    {
                        String token;
                        if (read_predefined<false>(input, symbol, token)) {
                            auto tmp = wrap::make_shared<Predef<String>>();
                            tmp->kind = TokenKind::symbol;
                            tmp->token = std::move(token);
                            to_next(tmp);
                            continue;
                        }
                    }
                    {
                        String token;
                        if (read_predefined<true>(input, keyword, token, symbol)) {
                            auto tmp = wrap::make_shared<Predef<String>>();
                            tmp->kind = TokenKind::keyword;
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
                            tmp->kind = TokenKind::context;
                            tmp->token = std::move(token);
                            tmp->layer = layer;
                            to_next(tmp);
                            continue;
                        }
                    }
                }
            }
        };
    }  // namespace tokenize
}  // namespace utils