/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// cast - define safe downcast
#pragma once

#include "token.h"
#include "space.h"
#include "line.h"
#include "predefined.h"
#include "identifier.h"
#include "comment.h"

namespace utils {

    namespace tokenize {
        namespace internal {
            template <class String, class T>
            struct CastHelper {
                static std::nullptr_t cast(auto&) {
                    return nullptr;
                }
            };

            template <class String>
            struct CastHelper<String, Predef<String>> {
                static Predef<String>* cast(auto& ptr) {
                    if (!ptr) {
                        return nullptr;
                    }
                    if (ptr->is(TokenKind::keyword) || ptr->is(TokenKind::symbol)) {
                        return static_cast<Predef<String>*>(std::addressof(*ptr));
                    }
                    return nullptr;
                }
            };

            template <class String>
            struct CastHelper<String, PredefCtx<String>> {
                static PredefCtx<String>* cast(auto& ptr) {
                    if (!ptr) {
                        return nullptr;
                    }
                    if (ptr->is(TokenKind::context)) {
                        return static_cast<PredefCtx<String>*>(std::addressof(*ptr));
                    }
                    return nullptr;
                }
            };

            template <class String>
            struct CastHelper<String, Identifier<String>> {
                static Identifier<String>* cast(auto& ptr) {
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
                static Line<String>* cast(auto& ptr) {
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
                static Space<String>* cast(auto& ptr) {
                    if (!ptr) {
                        return nullptr;
                    }
                    if (ptr->is(TokenKind::space)) {
                        return static_cast<Space<String>*>(std::addressof(*ptr));
                    }
                    return nullptr;
                }
            };

            template <class String>
            struct CastHelper<String, Comment<String>> {
                static Comment<String>* cast(auto& ptr) {
                    if (!ptr) {
                        return nullptr;
                    }
                    if (ptr->is(TokenKind::comment)) {
                        return static_cast<Comment<String>*>(std::addressof(*ptr));
                    }
                    return nullptr;
                }
            };
        }  // namespace internal

        template <template <class> class T, class String>
        T<String>* cast(wrap::shared_ptr<Token<String>>& ptr) {
            return internal::CastHelper<String, T<String>>::cast(ptr);
        }

        template <template <class> class T, class String>
        T<String>* cast(Token<String>* ptr) {
            return internal::CastHelper<String, T<String>>::cast(ptr);
        }

    }  // namespace tokenize
}  // namespace utils
