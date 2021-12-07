/*license*/

// cast - define safe downcast
#pragma once

#include "tokenizer.h"

namespace utils {

    namespace tokenize {
        namespace internal {
            template <class String, class T>
            struct CastHelper;

            template <class String>
            struct CastHelper<String, Predef<String>> {
                Predef<String>* cast(wrap::shared_ptr<Token<String>>& ptr) {
                    if (!ptr) {
                        return nullptr;
                    }
                    if (ptr->is(TokenKind::keyword) || ptr->is(TokenKind::symbol) || ptr->is(TokenKind::context)) {
                        return static_cast<Predef<String>*>(std::addressof(*ptr));
                    }
                    return nullptr;
                }
            };

            template <class String>
            struct CastHelper<String, Identifier<String>> {
                Identifier<String>* cast(wrap::shared_ptr<Token<String>>& ptr) {
                    if (!ptr) {
                        return nullptr;
                    }
                    if (ptr->is(TokenKind::identifier)) {
                        return static_cast<Identifier<String>*>(std::addressof(*ptr));
                    }
                    return nullptr;
                }
            };

            template <class String>
            struct CastHelper<String, Line<String>> {
                Line<String>* cast(wrap::shared_ptr<Token<String>>& ptr) {
                    if (!ptr) {
                        return nullptr;
                    }
                    if (ptr->is(TokenKind::line)) {
                        return static_cast<Line<String>*>(std::addressof(*ptr));
                    }
                    return nullptr;
                }
            };

            template <class String>
            struct CastHelper<String, Space<String>> {
                Space<String>* cast(wrap::shared_ptr<Token<String>>& ptr) {
                    if (!ptr) {
                        return nullptr;
                    }
                    if (ptr->is(TokenKind::space)) {
                        return static_cast<Space<String>*>(std::addressof(*ptr));
                    }
                    return nullptr;
                }
            };
        }  // namespace internal
    }      // namespace tokenize
}  // namespace utils