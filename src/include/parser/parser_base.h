/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parser_base - parser base class
#pragma once

#include "../core/sequencer.h"
#include "token.h"
#include "ifaces.h"

namespace utils {
    namespace parser {
        template <class String, class Kind, template <class...> class Vec>
        struct ParseResult {
            using token_t = Token<String, Kind, Vec>;
            wrap::shared_ptr<token_t> tok;
            bool fatal;
            error<String> err;
        };

        template <class String, class Str2>
        struct RawMsgError {
            Str2 msg;
            Pos pos_;
            String Error() {
                return utf::convert<String>(msg);
            }

            Pos pos() {
                return pos_;
            }
        };

        template <class String>
        struct TokenNotMatchError {
            String tok;
            Pos pos_;
            String Error() {
                String msg;
                helper::appends(msg, "expect ", tok, " but not");
                return msg;
            }

            Pos pos() {
                return pos_;
            }
        };

        template <class String>
        struct UnexpectedTokenError {
            String tok;
            Pos pos_;
            String Error() {
                String msg;
                helper::appends(msg, "unexpected token ", tok);
                return msg;
            }

            Pos pos() {
                return pos_;
            }
        };

        template <class String, template <class...> class Vec>
        struct ListError {
            Vec<error<String>> list;
            String Error() {
                String msg;
                bool f = false;
                for (auto& v : list) {
                    if (f) {
                        helper::append(msg, ": ");
                    }
                    helper::append(msg, v.Error());
                    f = true;
                }
                return msg;
            }

            Vec<error<String>>* get_list() {
                return std::addressof(list);
            }

            Pos pos() {
                return {};
            }
        };

        enum class ParserKind {
            null,
            token,
            space,
            line,
            anyother,
            func,
            or_,
            and_,
            repeat,
            allow_none,
            some_pattern,
            only_one,
            weak_ref,
            other,
        };

        template <class Input, class String, class Kind, template <class...> class Vec>
        struct Parser {
            using token_t = Token<String, Kind, Vec>;
            using result_t = ParseResult<String, Kind, Vec>;

            virtual result_t parse(Sequencer<Input>& input, Pos& posctx) {
                return {};
            }

            virtual bool none_is_not_error() const {
                return false;
            }

            virtual ParserKind declkind() const {
                return ParserKind::null;
            }
        };

    }  // namespace parser
}  // namespace utils
