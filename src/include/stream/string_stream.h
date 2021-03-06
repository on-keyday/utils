/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// string_stream - string parser stream
#pragma once
#include "stream.h"
#include "utf_stream.h"

namespace utils {
    namespace parser {
        namespace stream {
            template <class PrefixSuffix>
            struct StringChecker {
                bool endprefix;
                PrefixSuffix psfix;

                bool ok(UTFStat* stat, auto& str) {
                    if (stat->index == 0) {
                        endprefix = false;
                        if (!psfix.start_prefix(stat)) {
                            return false;
                        }
                        return true;
                    }
                    if (!endprefix) {
                        if (psfix.prefix(stat)) {
                            return true;
                        }
                        endprefix = true;
                    }
                    if (psfix.suffix(stat)) {
                        return false;
                    }
                    return true;
                }

                ErrorToken endok(Input& input, auto& str) {
                    return psfix.endok(input, str);
                }
            };

            template <class Escape>
            struct FixiedPrefixString {
                const char32_t* prefix_v;
                const char32_t* suffix_v;
                size_t prefix_index = 0;
                size_t suffix_index = 0;
                Escape esc;

                bool start_prefix(UTFStat* stat) {
                    prefix_index = 0;
                    suffix_index = 0;
                    return stat->C == prefix_v[0];
                }

                bool prefix(UTFStat* stat) {
                    prefix_index = stat->index;
                    if (prefix_v[stat->index] == 0) {
                        esc.start_escape(stat);
                        return false;
                    }
                    return stat->C == prefix_v[stat->index];
                }

                bool suffix(UTFStat* stat) {
                    if (prefix_v[prefix_index] != 0 ||
                        suffix_v[suffix_index] == 0) {
                        // operation succeeded
                        return true;
                    }
                    if (stat->C == suffix_v[suffix_index]) {
                        suffix_index++;
                    }
                    else {
                        suffix_index = 0;
                    }
                    // esc.escape can reset suffix_index
                    if (!esc.escape(stat, suffix_index, suffix_v[0])) {
                        // suspend operation
                        return true;
                    }
                    return false;
                }

                ErrorToken endok(Input& input, auto& str) {
                    if (prefix_v[prefix_index] != 0 ||
                        suffix_v[suffix_index] != 0) {
                        return simpleErrToken{"invalid string literal prefix or suffix"};
                    }
                    return esc.endok(input, str);
                }
            };

            enum class EscapeMode {
                escape,
                raw,
                no_escape,
            };

            struct DefaultEscaper {
                size_t escindex;
                char32_t verb;
                EscapeMode mode;

                static constexpr size_t invalid_index = ~0 - 1;
                void start_escape(UTFStat* stat) {
                    verb = 0;
                    escindex = invalid_index;
                }

                bool escape(UTFStat* stat, size_t& suffix_index, char32_t suffix_first) {
                    // expect verb
                    if (escindex == stat->index - 1) {
                        // \0
                        if (number::is_oct(stat->C)) {
                            verb = 'o';
                        }
                        // \\ \n \t \b \a etc...
                        else if (stat->C != 'x' &&
                                 stat->C != 'u') {
                            if (stat->C == suffix_first) {
                                suffix_index = 0;
                            }
                            escindex = invalid_index;
                            return true;
                        }
                        // \xff \u9232
                        else {
                            verb = stat->C;
                            return true;
                        }
                    }
                    // expect verb o,x,u
                    if (verb != 0) {
                        // offset from verb
                        const auto offset =
                            stat->index - escindex -
                            (verb != 'o');
                        bool check = false;
                        bool offset_end = false;
                        bool loose = false;
                        if (verb == 'x') {
                            check = number::is_hex(stat->C);
                            offset_end = offset == 2;
                        }
                        else if (verb == 'u') {
                            check = number::is_hex(stat->C);
                            offset_end = offset == 4;
                        }
                        else {  // verb=='o'
                            check = number::is_oct(stat->C);
                            offset_end = offset == 3;
                            loose = true;
                        }
                        if (!check || offset_end) {
                            // if offset_end is false
                            // and not loose num count,
                            // invalid escape sequence appeared
                            if (!offset_end && !loose) {
                                return false;
                            }
                            // reset escape state
                            escindex = invalid_index;
                            verb = 0;
                        }
                    }
                    // find escape '\'
                    if (mode == EscapeMode::escape &&
                        escindex == invalid_index &&
                        stat->C == '\\') {
                        escindex = stat->index;
                    }
                    if (mode != EscapeMode::raw &&
                        (stat->C == '\n' ||
                         stat->C == '\r')) {
                        // if mode!=EscapeMode::raw
                        // line is not allowed
                        return false;
                    }
                    return true;
                }

                ErrorToken endok(Input& input, auto& str) {
                    // when parsing string like "\1"
                    // verb maybe o
                    if (escindex != invalid_index &&
                        verb != 'o') {
                        return simpleErrToken{"invalid escape sequence"};
                    }
                    return {};
                }
            };

            constexpr auto tokenString = "string";

            template <class Str>
            struct StringToken {
                Str text;
                size_t pos;
                void token(PB pb) {
                    helper::append(pb, text);
                }

                TokenInfo info() {
                    TokenInfo stat{};
                    stat.kind = tokenString;
                    stat.dirtok = text.c_str();
                    stat.pos = pos;
                    stat.len = text.size();
                    return stat;
                }
            };

            template <class Str, class Check>
            struct StringParser {
                Check check;

                Token parse(Input& input) {
                    Str str;
                    auto pos = input.pos();
                    auto tok = read_default_utf(input, str, check);
                    if (has_err(tok)) {
                        return tok;
                    }
                    return StringToken<Str>{std::move(str), pos};
                }
            };

            template <class Str>
            auto make_default_string(const char32_t* begin, const char32_t* end, EscapeMode mode = EscapeMode::escape) {
                using check_t = StringChecker<FixiedPrefixString<DefaultEscaper>>;
                StringParser<Str, check_t> v{};
                v.check.psfix.prefix_v = begin;
                v.check.psfix.suffix_v = end;
                v.check.psfix.esc.mode = mode;
                return v;
            }

        }  // namespace stream
    }      // namespace parser
}  // namespace utils
