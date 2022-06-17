/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// semantics_stream - token with semantics stream
#pragma once
#include "utf_stream.h"
#include <number/char_range.h>

namespace utils {
    namespace parser {
        namespace stream {
            enum LookUpLevel {
                level_current,
                level_function,
                level_global,
                level_all,
            };

            constexpr auto tokenIdent = "identifier";

            template <class String>
            struct Identifier {
                String str;
                size_t pos;
                void token(PB pb) {
                    helper::append(pb, str);
                }
                TokenInfo info() {
                    TokenInfo info{};
                    info.kind = tokenIdent;
                    return info;
                }
            };

            enum class IDPolicy {
                not_exist_is_error,
                assignment_makes_new,
                anyway_create,
            };

            template <class String, class Check>
            struct IdentifierParser {
                Check check;
                Token parse(Input& input) {
                    String str;
                    auto pos = input.pos();
                    auto err = read_default_utf(input, str, check);
                    if (has_err(err)) {
                        return err;
                    }
                    err = Identifier<String>{std::move(str), pos};
                    // TODO(on-keyday): add semantics rule
                    return err;
                }
            };

            template <class Str>
            struct NumberError {
                const char* text;
                Str str;
                void token(PB) {}
                void error(PB pb) {
                    helper::appends("");
                }

                Error err() {
                    return *this;
                }

                bool kind_of(const char* v) {
                    return helper::equal(v, "number");
                }
            };

            constexpr auto tokenNumber = "number";

            template <class Str>
            struct Number {
                Str text;
                bool is_float;
                size_t pos;

                void token(PB pb) {
                    helper::append(pb, text);
                }

                TokenInfo info() {
                    TokenInfo info;
                    info.kind = tokenNumber;
                    info.dirtok = text.c_str();
                    info.len = text.size();
                    info.pos = pos;
                    return info;
                }
            };

            template <class Ident>
            struct NumberCheck {
                char32_t prev = 0;
                char32_t end = 0;
                int radix = 0;
                bool dot = false;
                bool exp = false;
                bool exp_op = false;
                Ident is_ident;

                bool prefix(char32_t C) {
                    if (C == 'x' || C == 'X') {
                        radix = 16;
                    }
                    else if (C == 'b' || C == 'B') {
                        radix = 2;
                    }
                    else if (C == 'o' || C == 'O') {
                        radix = 8;
                    }
                    else {
                        return false;
                    }
                    return true;
                }

                bool is_exp(char32_t C) const {
                    return radix == 10 && (C == 'e' || C == 'E') ||
                           radix == 16 && (C == 'p' || C == 'P');
                }

                bool ok(st::UTFStat* stat, auto& str) {
                    if (stat->index == 0) {
                        if (!number::is_digit(stat->C) && stat->C != '.') {
                            end = stat->C;
                            return false;
                        }
                        dot = false;
                        exp = false;
                        exp_op = false;
                        radix = 10;
                    }
                    if (stat->index == 1 && prev == '0') {
                        if (!prefix(stat->C) && stat->C != '.' && stat->C != 'e' && stat->C != 'E') {
                            end = stat->C;
                            return false;
                        }
                    }
                    if (stat->C == '.') {
                        if (exp || dot) {
                            end = stat->C;
                            return false;
                        }
                        dot = true;
                        prev = stat->C;
                        return true;
                    }
                    if (is_exp(stat->C)) {
                        if (exp || stat->C == 'x' || stat->C == 'X') {
                            end = stat->C;
                            return false;
                        }
                        exp = true;
                        prev = stat->C;
                        return true;
                    }
                    if (stat->C == '+' || stat->C == '-') {
                        if (!exp || exp_op || !is_exp(prev)) {
                            end = stat->C;
                            return false;
                        }
                        radix = 10;
                        exp_op = true;
                        prev = stat->C;
                        return true;
                    }
                    if (!number::is_in_ascii_range(stat->C) ||
                        !number::is_radix_char(stat->C, radix)) {
                        end = stat->C;
                        return false;
                    }
                    prev = stat->C;
                    return true;
                }

                Token endok(Input& input, auto& str) {
                    using Str = std::remove_cvref_t<decltype(str)>;
                    if (str.size() == 0) {
                        return NumberError<Str>{"expect number but not", str};
                    }
                    // 0x 0b 0o
                    if (str.size() == 2 && prefix(prev)) {
                        return NumberError<Str>{"expect number after prefix", str};
                    }
                    // 0e 0x0p
                    if (is_exp(prev)) {
                        return NumberError<Str>{"expect number after exp", str};
                    }
                    // 0e+ 0x0p-
                    if (prev == '+' || prev == '-') {
                        return NumberError<Str>{"expect number after exp + or -", str};
                    }
                    if (prev == '.') {
                        // .
                        if (str.size() == 1) {
                            return NumberError<Str>{"expect number after '.'", str};
                        }
                        // 0.. 0.method()
                        if (end == '.' || is_ident(end)) {
                            str.pop_back();
                            input.offset_seek(-1);
                            return {};
                        }
                        // 0.+2 0.*4 0.
                        else {
                            return {};
                        }
                    }
                    return {};
                }
            };

            template <class String, class Ident>
            struct NumberParser {
                NumberCheck<Ident> num;
                Token parse(Input& input) {
                    String str;
                    auto pos = input.pos();
                    auto err = read_default_utf(input, str, num);
                    if (has_err(err)) {
                        return err;
                    }
                    auto fl = num.dot || num.exp;
                    return Number<String>{std::move(str), fl, pos};
                }
            };

            template <class String, class Check>
            auto make_ident(Check check) {
                return IdentifierParser<String, Check>{std::move(check)};
            }
        }  // namespace stream
    }      // namespace parser
}  // namespace utils
