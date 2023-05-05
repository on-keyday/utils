/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "unicodedata.h"
#include "../../core/sequencer.h"
#include "../../number/parse.h"
#include "../../space/eol.h"
#include "../../helper/readutil.h"

namespace utils {
    namespace unicode::data::text {

        enum class ParseError {
            none,
            parse_hex,
            parse_dec,
            expect_semicolon,
            unexpected_eof,
            unexpected_mirror,
            expect_eol,
            expect_two_dot,
        };

        namespace internal {
            template <class T>
            ParseError parse_case(Sequencer<T>& seq, CaseMap& ca) {
                if (!seq.match(";")) {
                    if (!number::parse_integer(seq, ca.upper, 16)) {
                        return ParseError::parse_hex;
                    }
                    ca.flag |= flags::has_upper;
                }
                if (!seq.seek_if(";")) {
                    return ParseError::expect_semicolon;
                }
                if (!seq.match(";")) {
                    if (!number::parse_integer(seq, ca.lower, 16)) {
                        return ParseError::parse_hex;
                    }
                    ca.flag |= flags::has_lower;
                }
                if (!seq.seek_if(";")) {
                    return ParseError::expect_semicolon;
                }
                if (!space::parse_eol<false>(seq)) {
                    if (!number::parse_integer(seq, ca.title, 16)) {
                        return ParseError::parse_hex;
                    }
                    ca.flag |= flags::has_title;
                }
                return ParseError::none;
            }

            template <class T, class String>
            ParseError parse_decomposition(Sequencer<T>& seq, Decomposition<String>& res) {
                if (seq.seek_if(";")) {
                    return ParseError::none;
                }
                if (seq.match("<")) {
                    res.command.clear();
                    if (!helper::read_until(res.command, seq, ">")) {
                        return ParseError::unexpected_eof;
                    }
                    res.command.push_back('>');
                }
                while (seq.current() != ';') {
                    if (seq.current() == ' ') {
                        seq.consume();
                    }
                    char32_t val = 0;
                    if (!number::parse_integer(seq, val, 16)) {
                        return ParseError::parse_hex;
                    }
                    res.to.push_back(val);
                }
                if (!seq.seek_if(";")) {
                    return ParseError::expect_semicolon;
                }
                return ParseError::none;
            }

            template <class T>
            ParseError parse_real(Sequencer<T>& seq, Numeric& numeric) {
                if (seq.seek_if("-")) {
                    numeric.flag |= flags::sign;
                }
                if (!seq.eos()) {
                    const size_t pos = seq.rptr;
                    auto ok = number::parse_integer(seq, numeric.v3_S._1, 10);
                    if (!ok && ok != number::NumError::overflow) {
                        return ParseError::parse_dec;
                    }
                    if (ok == number::NumError::overflow) {
                        seq.rptr = pos;
                        if (!number::parse_integer(seq, numeric.v3_L, 10)) {
                            return ParseError::parse_dec;
                        }
                        numeric.flag |= flags::large_num;
                    }
                    else {
                        numeric.flag |= flags::exist_one;
                        if (seq.seek_if("/")) {
                            if (!number::parse_integer(seq, numeric.v3_S._2, 10)) {
                                return ParseError::parse_dec;
                            }
                            numeric.flag |= flags::exist_two;
                        }
                    }
                }
                return ParseError::none;
            }

            template <class T, class String>
            ParseError parse_numeric(Sequencer<T>& seq, CodeInfo<String>& info) {
                if (!seq.match(";")) {
                    if (!number::parse_integer(seq, info.numeric.v1, 10)) {
                        return ParseError::parse_dec;
                    }
                    info.numeric.flag |= flags::has_digit;
                }
                if (!seq.seek_if(";")) {
                    return ParseError::expect_semicolon;
                }
                if (!seq.match(";")) {
                    if (!number::parse_integer(seq, info.numeric.v2, 10)) {
                        return ParseError::parse_dec;
                    }
                    info.numeric.flag |= flags::has_decimal;
                }
                if (!seq.seek_if(";")) {
                    return ParseError::expect_semicolon;
                }
                if (seq.seek_if(";")) {
                    return ParseError::none;
                }
                if (auto err = parse_real(seq, info.numeric); err != ParseError::none) {
                    return ParseError::none;
                }
                if (!seq.seek_if(";")) {
                    return ParseError::expect_semicolon;
                }
                return ParseError::none;
            }

            template <class T>
            bool read_until_semicolon(Sequencer<T>& seq, auto& out) {
                return helper::read_until(out, seq, ";");
            }

            template <class T, class String>
            ParseError parse_codepoint(Sequencer<T>& seq, CodeInfo<String>& info) {
                if (!number::parse_integer(seq, info.codepoint, 16)) {
                    return ParseError::parse_hex;
                }
                if (!seq.seek_if(";")) {
                    return ParseError::expect_semicolon;
                }
                if (!read_until_semicolon(seq, info.name) ||
                    !read_until_semicolon(seq, info.category)) {
                    return ParseError::expect_semicolon;
                }
                if (!number::parse_integer(seq, info.ccc, 10)) {
                    return ParseError::parse_hex;
                }
                if (!seq.seek_if(";")) {
                    return ParseError::expect_semicolon;
                }
                if (!read_until_semicolon(seq, info.bidiclass)) {
                    return ParseError::expect_semicolon;
                }
                if (auto err = parse_decomposition(seq, info.decomposition); err != ParseError::none) {
                    return err;
                }
                if (auto err = parse_numeric(seq, info); err != ParseError::none) {
                    return err;
                }
                if (seq.seek_if("Y;")) {
                    info.mirrored = true;
                }
                else if (seq.seek_if("N;")) {
                    info.mirrored = false;
                }
                else {
                    return ParseError::unexpected_mirror;
                }
                if (!read_until_semicolon(seq, helper::nop)) {
                    return ParseError::expect_semicolon;
                }
                if (!read_until_semicolon(seq, helper::nop)) {
                    return ParseError::expect_semicolon;
                }
                if (auto err = parse_case(seq, info.casemap); err != ParseError::none) {
                    return err;
                }
                if (!space::parse_eol<true>(seq)) {
                    return ParseError::expect_eol;
                }
                data::internal::guess_east_asian_width(info);
                return ParseError::none;
            }

        }  // namespace internal

        template <class T, class String>
        ParseError parse_unicodedata_text(Sequencer<T>& seq, UnicodeData<String>& ret) {
            CodeInfo<String>* prev = nullptr;
            while (!seq.eos()) {
                CodeInfo<String> info;
                if (auto err = internal::parse_codepoint(seq, info); err != ParseError::none) {
                    return err;
                }
                data::internal::set_codepoint_info(info, ret, prev);
            }
            return ParseError::none;
        }

        namespace internal {

            bool skip_line(auto& seq) {
                helper::read_whilef(helper::nop, seq, [](auto c) { return c != '\r' && c != '\n'; });
                return space::parse_eol(seq);
            }

            ParseError read_range(std::uint32_t& begin, std::uint32_t& end, auto& seq, bool must) {
                if (!number::parse_integer(seq, begin, 16)) {
                    return ParseError::parse_hex;
                }
                if (!seq.seek_if("..")) {
                    if (must) {
                        return ParseError::expect_two_dot;
                    }
                    end = begin;
                    return ParseError::none;
                }
                if (!number::parse_integer(seq, end, 16)) {
                    return ParseError::parse_hex;
                }
                return ParseError::none;
            }
        }  // namespace internal

        template <class T, class String>
        ParseError parse_blocks_text(Sequencer<T>& seq, UnicodeData<String>& data) {
            while (!seq.eos()) {
                if (seq.current() == '#' || !number::is_hex(seq.current())) {
                    if (!internal::skip_line(seq)) {
                        return ParseError::unexpected_eof;
                    }
                    continue;
                }
                std::uint32_t begin, end;
                std::string block_name;
                if (auto err = internal::read_range(begin, end, seq, true); err != ParseError::none) {
                    return err;
                }
                if (!seq.seek_if("; ")) {
                    return ParseError::expect_semicolon;
                }
                if (!helper::read_whilef(block_name, seq, [](auto c) { return c != '\r' && c != '\n'; })) {
                    return ParseError::unexpected_eof;
                }
                if (!internal::skip_line(seq)) {
                    return ParseError::unexpected_eof;
                }
                for (auto i = begin; i <= end; i++) {
                    if (auto found = data.codes.find(i); found != data.codes.end()) {
                        CodeInfo<String>& info = (*found).second;
                        info.block = block_name;
                        data.blocks.emplace(block_name, &info);
                    }
                }
            }
            return ParseError::none;
        }

        template <class T, class String>
        ParseError parse_east_asian_width_text(Sequencer<T>& seq, UnicodeData<String>& data) {
            while (!seq.eos()) {
                if (seq.current() == '#' || !number::is_hex(seq.current())) {
                    if (!internal::skip_line(seq)) {
                        return ParseError::unexpected_eof;
                    }
                    continue;
                }
                std::uint32_t begin, end;
                std::string width;
                if (auto err = internal::read_range(begin, end, seq, false); err != ParseError::none) {
                    return err;
                }
                if (!seq.seek_if(";")) {
                    return ParseError::expect_semicolon;
                }
                if (!helper::read_until(width, seq, " ")) {
                    return ParseError::unexpected_eof;
                }
                if (!internal::skip_line(seq)) {
                    return ParseError::unexpected_eof;
                }
                for (auto i = begin; i <= end; i++) {
                    if (auto found = data.codes.find(i); found != data.codes.end()) {
                        CodeInfo<String>& info = (*found).second;
                        info.east_asian_width = width;
                    }
                }
            }
            return ParseError::none;
        }

        template <class T, class String>
        ParseError parse_emoji_data_text(Sequencer<T>& seq, UnicodeData<String>& data) {
            while (!seq.eos()) {
                if (seq.current() == '#' || !number::is_hex(seq.current())) {
                    if (!internal::skip_line(seq)) {
                        return ParseError::unexpected_eof;
                    }
                    continue;
                }
                std::uint32_t begin, end;
                std::string emoji;
                if (auto err = internal::read_range(begin, end, seq, false); err != ParseError::none) {
                    return err;
                }
                helper::read_until<false>(helper::nop, seq, ";");
                if (!seq.seek_if("; ")) {
                    return ParseError::expect_semicolon;
                }
                if (!helper::read_whilef(emoji, seq, [](auto c) { return c != ' ' && c != '#'; })) {
                    return ParseError::unexpected_eof;
                }
                if (!internal::skip_line(seq)) {
                    return ParseError::unexpected_eof;
                }
                for (auto i = begin; i <= end; i++) {
                    if (auto found = data.codes.find(i); found != data.codes.end()) {
                        CodeInfo<String>& info = (*found).second;
                        if (emoji == "") {
                            ;
                        }
                        info.emoji_data.push_back(emoji);
                    }
                }
            }
            return ParseError::none;
        }

    }  // namespace unicode::data::text
}  // namespace utils
