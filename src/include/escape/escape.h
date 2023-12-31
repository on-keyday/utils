/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
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
#include "../core/byte.h"
#include <strutil/append.h>
#include <array>

namespace utils {
    namespace escape {
        // priority utf16 > utf32 > hex > oct
        enum class EscapeFlag : byte {
            none = 0,
            utf16 = 0x1,            // \u0000
            oct = 0x2,              // \000
            hex = 0x4,              // \x00
            utf32 = 0x8,            // \U00000000
            upper = 0x10,           // using upper case for hex escape
            no_replacement = 0x20,  // not replace invalid char to replacement character
            all = utf16 | oct | hex,
        };

        DEFINE_ENUM_FLAGOP(EscapeFlag)
        template <size_t num>
        using escape_set = std::array<std::pair<char, const char*>, num>;
        constexpr auto default_set() {
            return escape_set<11>{
                {
                    {'\n', "n"},
                    {'\r', "r"},
                    {'\t', "t"},
                    {'\a', "a"},
                    {'\b', "b"},
                    {'\v', "v"},
                    {'\033' /*\e (for msvc)*/, "e"},
                    {'\f', "f"},
                    {'\\', "\\"},
                    {'\"', "\""},
                    {'\'', "'"},
                },
            };
        }

        constexpr auto json_set(bool upper = false) {
            return escape_set<10>{
                {
                    {'\n', "n"},
                    {'\r', "r"},
                    {'\t', "t"},
                    {'\b', "b"},
                    {'\f', "f"},
                    {'\\', "\\"},
                    {'\"', "\""},
                    {'/', "/"},
                    {'<', upper ? "u003C" : "u003c"},
                    {'>', upper ? "u003E" : "u003e"},
                },
            };
        }

        constexpr auto json_set_no_html() {
            return escape_set<7>{
                {
                    {'\n', "n"},
                    {'\r', "r"},
                    {'\t', "t"},
                    {'\b', "b"},
                    {'\f', "f"},
                    {'\\', "\\"},
                    {'\"', "\""},
                },
            };
        }

        constexpr auto default_should_escape() {
            return [](auto&& c) {
                return !number::is_in_visible_range(c) && c != ' ';
            };
        };

        constexpr auto html_range() {
            return [](auto&& c) {
                constexpr auto defrange = default_should_escape();
                return defrange(c) || c == '<' || c == '>';
            };
        }

        template <class In, class Out, class Escape = decltype(default_set()), class Range = decltype(default_should_escape())>
        constexpr number::NumErr escape_str(Sequencer<In>& seq, Out& out, EscapeFlag flag = EscapeFlag::none,
                                            Escape&& esc = default_set(), Range&& should_escape = default_should_escape()) {
            auto flush_hex_number = [&](auto n) -> number::NumErr {
                number::ToStrFlag f = number::ToStrFlag::none;
                if (any(flag & EscapeFlag::upper)) {
                    f = number::ToStrFlag::upper;
                }
                if (auto e = number::to_string(out, n, 16, f); !e) {
                    return e;
                }
                return true;
            };
            while (!seq.eos()) {
                std::make_unsigned_t<decltype(seq.current())> c = seq.current();
                bool done = false;
                for (auto& s : esc) {
                    if (get<0>(s) == c) {
                        out.push_back('\\');
                        strutil::append(out, get<1>(s));
                        done = true;
                        break;
                    }
                }
                if (!done) {
                    if (should_escape(c)) {
                        if (any(flag & EscapeFlag::utf16)) {
                            auto s = seq.rptr;
                            utf::U16Buffer buf;
                            if (utf::convert_one(seq, buf, false, !any(flag & EscapeFlag::no_replacement))) {
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
                                    return flush_hex_number(n);
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
                        if (any(flag & EscapeFlag::utf32)) {
                            auto s = seq.rptr;
                            std::uint32_t v;
                            if (utf::to_utf32(seq, v, false, !any(flag & EscapeFlag::no_replacement))) {
                                auto set_one = [&](auto n) -> number::NumErr {
                                    strutil::append(out, "\\U");
                                    if (n < 0x10000009) {
                                        out.push_back('0');
                                    }
                                    if (n < 0x1000000) {
                                        out.push_back('0');
                                    }
                                    if (n < 0x100000) {
                                        out.push_back('0');
                                    }
                                    if (n < 0x10000) {
                                        out.push_back('0');
                                    }
                                    if (n < 0x1000) {
                                        out.push_back('0');
                                    }
                                    if (n < 0x100) {
                                        out.push_back('0');
                                    }
                                    if (n < 0x10) {
                                        out.push_back('0');
                                    }
                                    return flush_hex_number(n);
                                };
                                if (auto e = set_one(v); !e) {
                                    return e;
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
                            if (auto e = flush_hex_number(c); !e) {
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
                        if (!done && !any(flag & EscapeFlag::no_replacement)) {
                            if (utf::convert_one(seq, out, false, true)) {
                                done = true;
                                seq.backto();
                            }
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

        template <class In, class Out, class Escape = decltype(default_set()), class Range = decltype(default_should_escape())>
        constexpr number::NumErr escape_str(In&& in, Out& out, EscapeFlag flag = EscapeFlag::none,
                                            Escape&& esc = default_set(), Range&& range = default_should_escape()) {
            auto seq = make_ref_seq(in);
            return escape_str(seq, out, flag, esc, range);
        }

        template <class Out, class In, class Escape = decltype(default_set()), class Range = decltype(default_should_escape())>
        constexpr Out escape_str(In&& seq, EscapeFlag flag = EscapeFlag::none,
                                 Escape&& esc = default_set(), Range&& range = default_should_escape()) {
            Out out{};
            escape_str(seq, out, flag, esc, range);
            return out;
        }

        template <class In, class Out, class Escape = decltype(default_set())>
        constexpr number::NumErr unescape_str(Sequencer<In>& seq, Out& out, Escape&& esc = default_set(), EscapeFlag flag = EscapeFlag::all) {
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
                        if (seq.seek_if(get<1>(s))) {
                            out.push_back(get<0>(s));
                            seq.backto();
                            done = true;
                            break;
                        }
                    }
                    if (!done) {
                        if (c == 'x' && any(flag & EscapeFlag::hex)) {
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
                        else if (c == 'u' && any(flag & EscapeFlag::utf16)) {
                            seq.consume();
                            if (seq.eos()) {
                                return false;
                            }
                            utf::U16Buffer buf;
                            std::uint16_t i;
                            if (auto e = number::read_limited_int<4>(seq, i, 16, true); !e) {
                                return e;
                            }
                            buf.push_back(i);
                            if (unicode::utf16::is_high_surrogate(i)) {
                                auto p = seq.rptr;
                                if (seq.seek_if("\\u")) {
                                    if (auto e = number::read_limited_int<4>(seq, i, 16, true); !e) {
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
                        else if (c == 'U' && any(flag & EscapeFlag::utf32)) {
                            seq.consume();
                            if (seq.eos()) {
                                return false;
                            }
                            utf::U16Buffer buf;
                            std::uint16_t i;
                            if (auto e = number::read_limited_int<8>(seq, i, 16, true); !e) {
                                return e;
                            }
                            buf.push_back(i);
                            if (unicode::utf16::is_high_surrogate(i)) {
                                auto p = seq.rptr;
                                if (seq.seek_if("\\U")) {
                                    if (auto e = number::read_limited_int<8>(seq, i, 16, true); !e) {
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
                        else if (number::is_oct(c) && any(flag & EscapeFlag::oct)) {
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
        constexpr number::NumErr unescape_str(In&& in, Out& out, Escape&& esc = default_set(), EscapeFlag flag = EscapeFlag::all) {
            auto seq = make_ref_seq(in);
            return unescape_str(seq, out, esc, flag);
        }

        template <class Out, class In, class Escape = decltype(default_set())>
        constexpr Out unescape_str(In&& in, Escape&& esc = default_set(), EscapeFlag flag = EscapeFlag::all) {
            Out out{};
            unescape_str(in, out, esc, flag);
            return out;
        }

    }  // namespace escape
}  // namespace utils
