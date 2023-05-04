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

            template <class T>
            ParseError parse_decomposition(Sequencer<T>& seq, Decomposition& res) {
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

            template <class T>
            ParseError parse_numeric(Sequencer<T>& seq, CodeInfo& info) {
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

            template <class T>
            ParseError parse_codepoint(Sequencer<T>& seq, CodeInfo& info) {
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

        template <class T>
        ParseError parse_unicodedata_text(Sequencer<T>& seq, UnicodeData& ret) {
            CodeInfo* prev = nullptr;
            while (!seq.eos()) {
                CodeInfo info;
                if (auto err = internal::parse_codepoint(seq, info); err != ParseError::none) {
                    return err;
                }
                data::internal::set_codepoint_info(info, ret, prev);
            }
            return ParseError::none;
        }

        template <class T>
        ParseError parse_blocks_text(Sequencer<T>& seq, UnicodeData& data) {
            while (!seq.eos()) {
                if (seq.current() == '#' || !number::is_hex(seq.current())) {
                    helper::read_whilef(helper::nop, seq, [](auto c) { return c != '\r' && c != '\n'; });
                    if (!space::parse_eol(seq)) {
                        return ParseError::expect_eol;
                    }
                    continue;
                }
                std::uint32_t begin, end;
                std::string block_name;
                if (!number::parse_integer(seq, begin, 16)) {
                    return ParseError::parse_hex;
                }
                if (!seq.seek_if("..")) {
                    return ParseError::expect_two_dot;
                }
                if (!number::parse_integer(seq, end, 16)) {
                    return ParseError::parse_hex;
                }
                if (!seq.seek_if("; ")) {
                    return ParseError::expect_semicolon;
                }
                helper::read_whilef(block_name, seq, [](auto c) { return c != '\r' && c != '\n'; });
                if (!space::parse_eol(seq)) {
                    return ParseError::expect_eol;
                }
                for (auto i = begin; i <= end; i++) {
                    if (auto found = data.codes.find(i); found != data.codes.end()) {
                        CodeInfo& info = (*found).second;
                        info.block = block_name;
                    }
                }
            }
            return ParseError::none;
        }

        template <class T>
        ParseError parse_east_asian_width_text(Sequencer<T>& seq, UnicodeData& data) {
            while (!seq.eos()) {
                if (seq.current() == '#' || !number::is_hex(seq.current())) {
                    helper::read_whilef(helper::nop, seq, [](auto c) { return c != '\r' && c != '\n'; });
                    if (!space::parse_eol(seq)) {
                        return ParseError::expect_eol;
                    }
                    continue;
                }
                std::uint32_t begin, end;
                std::string width;
                if (!number::parse_integer(seq, begin, 16)) {
                    return ParseError::parse_hex;
                }
                if (seq.seek_if("..")) {
                    if (!number::parse_integer(seq, end, 16)) {
                        return ParseError::parse_hex;
                    }
                }
                else {
                    end = begin;
                }
                if (!seq.seek_if(";")) {
                    return ParseError::expect_semicolon;
                }
                if (!helper::read_until(width, seq, ' ')) {
                    return ParseError::unexpected_eof;
                }
                helper::read_whilef(helper::nop, seq, [](auto c) { return c != '\r' && c != '\n'; });
                if (!space::parse_eol(seq)) {
                    return ParseError::expect_eol;
                }
                for (auto i = begin; i <= end; i++) {
                    if (auto found = data.codes.find(i); found != data.codes.end()) {
                        CodeInfo& info = (*found).second;
                        info.east_asian_width = width;
                    }
                }
            }
            return ParseError::none;
        }

    }  // namespace unicode::data::text
}  // namespace utils
