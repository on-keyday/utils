/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// escape - string escape sequence
#pragma once

#include "../core/sequencer.h"
#include "../strutil/append.h"
#include "../unicode/utf/convert.h"
#include "../unicode/utf/minibuffer.h"
#include "../number/char_range.h"
#include "../number/to_string.h"
#include "../number/parse.h"
#include <array>

namespace utils {
    namespace escape {
        // priority utf > hex > oct
        enum class EscapeFlag {
            none = 0,
            utf = 0x1,  // \u0000
            oct = 0x2,  // \000
            hex = 0x4,  // \x00
        };

        DEFINE_ENUM_FLAGOP(EscapeFlag)
        template <size_t num>
        using escape_set = std::array<std::pair<char, char>, num>;
        constexpr auto default_set() {
            return escape_set<11>{
                {
                    {'\n', 'n'},
                    {'\r', 'r'},
                    {'\t', 't'},
                    {'\a', 'a'},
                    {'\b', 'b'},
                    {'\v', 'v'},
                    {'\033' /*\e (for msvc)*/, 'e'},
                    {'\f', 'f'},
                    {'\\', '\\'},
                    {'\"', '\"'},
                    {'\'', '\''},
                },
            };
        }

        constexpr auto json_set() {
            return escape_set<8>{
                {
                    {'\n', 'n'},
                    {'\r', 'r'},
                    {'\t', 't'},
                    {'\b', 'b'},
                    {'\f', 'f'},
                    {'\\', '\\'},
                    {'\"', '\"'},
                    {'/', '/'},
                },
            };
        }

        constexpr auto json_set_without_slash() {
            return escape_set<7>{
                {
                    {'\n', 'n'},
                    {'\r', 'r'},
                    {'\t', 't'},
                    {'\b', 'b'},
                    {'\f', 'f'},
                    {'\\', '\\'},
                    {'\"', '\"'},
                },
            };
        }

        constexpr auto default_range() {
            return [](auto&& c) {
                return c != ' ' && !number::is_in_visible_range(c);
            };
        };

        constexpr auto html_range() {
            return [](auto&& c) {
                constexpr auto defrange = default_range();
                return defrange(c) || c == '<' || c == '>';
            };
        }

        template <class In, class Out, class Escape = decltype(default_set()), class Range = decltype(default_range())>
        constexpr number::NumErr escape_str(Sequencer<In>& seq, Out& out, EscapeFlag flag = EscapeFlag::none,
                                            Escape&& esc = default_set(), Range&& range = default_range()) {
            while (!seq.eos()) {
                std::make_unsigned_t<decltype(seq.current())> c = seq.current();
                bool done = false;
                for (auto& s : esc) {
                    if (get<0>(s) == c) {
                        out.push_back('\\');
                        out.push_back(get<1>(s));
                        done = true;
                        break;
                    }
                }
                if (!done) {
                    if (range(c)) {
                        if (any(flag & EscapeFlag::utf)) {
                            auto s = seq.rptr;
                            utf::U16Buffer buf;
                            if (utf::convert_one(seq, buf)) {
                                auto set_one = [&](auto n) -> number::NumErr {
                                    strutil::append(out, "\\u");
                                    if (n < 0x1000) {
                                        out.push_back('0');
                                    }
                                    if (n < 0x100) {
                                        out.push_back('0');
                                    }
                                    if (n < 0x10) {
                                        out.push_back('0');
                                    }
                                    if (auto e = number::to_string(out, n, 16); !e) {
                                        return e;
                                    }
                                    return true;
                                };
                                for (size_t i = 0; i < buf.size(); i++) {
                                    if (auto e = set_one(buf[i]); !e) {
                                        return e;
                                    }
                                }
                                done = true;
                                seq.backto();
                            }
                            else {
                                seq.rptr = s;
                            }
                        }
                        if (!done && any(flag & EscapeFlag::hex)) {
                            strutil::append(out, "\\x");
                            if (c < 0x10) {
                                out.push_back('0');
                            }
                            if (auto e = number::to_string(out, c, 16); !e) {
                                return e;
                            }
                            done = true;
                        }
                        if (!done && any(flag & EscapeFlag::oct)) {
                            strutil::append(out, "\\");
                            if (auto e = number::to_string(out, c, 8); !e) {
                                return e;
                            }
                            done = true;
                        }
                        if (!done) {
                            out.push_back(c);
                        }
                    }
                    else {
                        out.push_back(c);
                    }
                }
                seq.consume();
            }
            return true;
        }

        template <class In, class Out, class Escape = decltype(default_set()), class Range = decltype(default_range())>
        constexpr number::NumErr escape_str(In&& in, Out& out, EscapeFlag flag = EscapeFlag::none,
                                            Escape&& esc = default_set(), Range&& range = default_range()) {
            auto seq = make_ref_seq(in);
            return escape_str(seq, out, flag, esc, range);
        }

        template <class In, class Out, class Escape = decltype(default_set())>
        constexpr number::NumErr unescape_str(Sequencer<In>& seq, Out& out, Escape&& esc = default_set()) {
            constexpr auto mx = (std::numeric_limits<std::make_unsigned_t<
                                     typename Sequencer<In>::char_type>>::max)();
            while (!seq.eos()) {
                auto c = seq.current();
                if (c == '\\') {
                    seq.consume();
                    if (seq.eos()) {
                        return false;
                    }
                    c = seq.current();
                    bool done = false;
                    for (auto& s : esc) {
                        if (get<1>(s) == c) {
                            out.push_back(get<0>(s));
                            done = true;
                            break;
                        }
                    }
                    if (!done) {
                        if (c == 'x') {
                            seq.consume();
                            if (seq.eos()) {
                                return false;
                            }
                            std::uint16_t i;
                            if (auto e = number::read_limited_int<4>(seq, i, 16); !e) {
                                return e;
                            }
                            if (i > mx) {
                                return number::NumError::overflow;
                            }
                            out.push_back(i);
                        }
                        else if (c == 'u') {
                            seq.consume();
                            if (seq.eos()) {
                                return false;
                            }
                            utf::U16Buffer buf;
                            std::uint16_t i;
                            if (auto e = number::read_limited_int<4>(seq, i, 16); !e) {
                                return e;
                            }
                            buf.push_back(i);
                            if (unicode::utf16::is_high_surrogate(i)) {
                                auto p = seq.rptr;
                                if (seq.seek_if("\\u")) {
                                    if (auto e = number::read_limited_int<4>(seq, i, 16); !e) {
                                        return e;
                                    }
                                    if (!unicode::utf16::is_low_surrogate(i)) {
                                        seq.rptr = p;
                                    }
                                    else {
                                        buf.push_back(i);
                                    }
                                }
                            }
                            if (!utf::convert(buf, out)) {
                                return false;
                            }
                            seq.backto();
                        }
                        else if (number::is_oct(c)) {
                            std::uint8_t i;
                            if (auto e = number::read_limited_int<3>(seq, i, 8); !e) {
                                return e;
                            }
                            out.push_back(i);
                            seq.backto();
                        }
                        else {
                            out.push_back(c);
                        }
                    }
                }
                else {
                    out.push_back(c);
                }
                seq.consume();
            }
            return true;
        }

        template <class In, class Out, class Escape = decltype(default_set())>
        constexpr number::NumErr unescape_str(In&& in, Out& out, Escape&& esc = default_set()) {
            auto seq = make_ref_seq(in);
            return unescape_str(seq, out, esc);
        }

    }  // namespace escape
}  // namespace utils
