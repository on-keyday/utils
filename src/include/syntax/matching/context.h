/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// context - matching context
#pragma once
#include "../make_parser/element.h"

#include "../../wrap/pack.h"

#include "../make_parser/keyword.h"

namespace utils {
    namespace syntax {

        template <class String, template <class...> class Vec>
        struct ErrorContext {
            Reader<String> r;
            wrap::internal::Pack err;
            wrap::shared_ptr<tknz::Token<String>> errat;
            wrap::shared_ptr<Element<String, Vec>> errelement;
        };

        template <class String>
        struct MatchResult {
            String token;
            KeyWord kind = KeyWord::id;
        };

        template <class String, template <class...> class Vec>
        struct MatchContext {
            ErrorContext<String, Vec>& err;
            const Vec<String>& stack;
            MatchResult<String> result;

            template <class Input>
            void error(Input&& input) {
                err.err << "error: " << input << "\n";
            }

            const char* what() const {
                return keywordv(result.kind);
            }

            const String& token() const {
                return result.token;
            }

            const String& top() const {
                return stack[stack.size() - 1];
            }

            KeyWord kind() const {
                return result.kind;
            }

            bool under(const String& v) const {
                for (auto i = stack.size() - 1; i != -1; i--) {
                    if (stack[i] == v) {
                        return true;
                    }
                }
                return false;
            }
        };
    }  // namespace syntax
}  // namespace utils
