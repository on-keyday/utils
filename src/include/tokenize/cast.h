/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
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
#include <memory>

namespace utils {

    namespace tokenize {
        namespace internal {
            template <class String, template <class> class T>
            struct CastHelper {
                static std::nullptr_t cast(auto&) {
                    return nullptr;
                }
            };

            template <class String>
            struct CastHelper<String, Predef> {
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
            struct CastHelper<String, PredefCtx> {
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
            struct CastHelper<String, Identifier> {
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
            struct CastHelper<String, Line> {
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
            struct CastHelper<String, Space> {
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
            struct CastHelper<String, Comment> {
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
            auto p = std::addressof(*ptr);
            return internal::CastHelper<String, T>::cast(p);
        }

        template <template <class> class T, class String>
        T<String>* cast(Token<String>* ptr) {
            return internal::CastHelper<String, T>::cast(ptr);
        }

    }  // namespace tokenize
}  // namespace utils
