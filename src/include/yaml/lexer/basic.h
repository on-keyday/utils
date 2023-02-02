/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "lexer.h"
#include "../../unicode/utf/convert.h"
#include "../../number/char_range.h"

namespace utils::yaml::lexer {

    struct BasicLexer : LexerCommon {
        using LexerCommon::LexerCommon;

        constexpr view::rvec read_line_break() {
            auto seq = remain();
            if (seq.seek_if("\r\n")) {
                return read(2);
            }
            else if (seq.seek_if("\r") || seq.seek_if("\n")) {
                return read(1);
            }
            return {};
        }

        constexpr view::rvec read_whites() {
            auto seq = remain();
            while (seq.current() == ' ' || seq.current() == '\t') {
                seq.consume();
            }
            return read(seq.rptr);
        }

        constexpr bool is_start_of_line() {
            auto al = r.read();
            if (al.size() == 0) {
                return true;
            }
            if (al.back() == '\r' || al.back() == '\n') {
                return true;
            }
            return false;
        }

        constexpr view::rvec read_separate_in_line() {
            if (auto v = read_whites(); v.size() != 0) {
                return v;
            }
            if (is_start_of_line()) {
                return read(0);
            }
            return {};
        }

        constexpr view::rvec read_indent(size_t n) {
            if (n == 0) {
                return read(0);
            }
            const auto begin = r.offset();
            auto seq = remain();
            while (seq.rptr < n && seq.current() == ' ') {
                seq.consume();
            }
            if (seq.rptr != n) {
                r.reset(begin);
                return {};
            }
            return read(seq.rptr);
        }

        constexpr view::rvec read_indent_less_than(size_t n) {
            if (n == 0) {
                return {};
            }
            const auto begin = r.offset();
            auto seq = remain();
            while (seq.rptr < n && seq.current() == ' ') {
                seq.consume();
            }
            if (seq.rptr == n) {
                r.reset(begin);
                return {};
            }
            return read(seq.rptr);
        }

        constexpr view::rvec read_flow_line_prefix(size_t n) {
            const auto begin = r.offset();
            if (read_indent(n).null()) {
                return {};
            }
            read_separate_in_line();  // optional
            return r.read().substr(begin);
        }

        constexpr view::rvec read_line_prefix(size_t n, Place c) {
            switch (c) {
                case Place::block_out:
                case Place::block_in:
                    return read_indent(n);
                case Place::flow_in:
                case Place::flow_out:
                    return read_flow_line_prefix(n);
                default:
                    return {};
            }
        }

        constexpr view::rvec read_empty_line(size_t n, Place c) {
            const auto begin = r.offset();
            if (read_line_prefix(n, c).null()) {
                read_indent_less_than(n);
            }
            if (read_line_break().null()) {
                r.reset(begin);
                return {};
            }
            return r.read().substr(begin);
        }

        constexpr view::rvec read_trimmed(size_t n, Place c) {
            const auto begin = r.offset();
            if (read_line_break().null()) {
                return {};
            }
            if (read_empty_line(n, c).null()) {
                r.reset(begin);
                return {};
            }
            while (!read_empty_line(n, c).null()) {
                // repeat
            }
            return r.read().substr(begin);
        }

        constexpr view::rvec read_folded(size_t n, Place c) {
            auto trim = read_trimmed(n, c);
            if (!trim.null()) {
                return trim;
            }
            return read_line_break();
        }

        constexpr view::rvec read_flow_folded(size_t n) {
            const auto begin = r.offset();
            read_separate_in_line();  // optioanl
            if (read_folded(n, Place::flow_in).null()) {
                r.reset(begin);
                return {};
            }
            if (read_flow_line_prefix(n).null()) {
                r.reset(begin);
                return {};
            }
            return r.read();
        }

       private:
        constexpr view::rvec read_chars(auto&& stop) {
            auto seq = remain();
            while (!seq.eos()) {
                const auto cur = seq.rptr;
                std::uint32_t code;
                if (!utf::to_utf32(seq, code)) {
                    return {};
                }
                if (stop(code)) {
                    return read(cur);
                }
            }
            return read(seq.rptr);
        }

       public:
        constexpr view::rvec read_nb_chars() {
            return read_chars([](std::uint32_t code) {
                return code == '\r' || code == '\n' ||
                       code == unicode::byte_order_mark ||
                       !is_pritable(code);
            });
        }

        constexpr view::rvec read_digits() {
            auto seq = remain();
            while (number::is_digit(seq.current())) {
                seq.consume();
            }
            return read(seq.rptr);
        }

        constexpr view::rvec read_hex_digits() {
            auto seq = remain();
            while (number::is_hex(seq.current())) {
                seq.consume();
            }
            return read(seq.rptr);
        }

        constexpr view::rvec read_ascii_letters() {
            auto seq = remain();
            while (number::is_alpha(seq.current())) {
                seq.consume();
            }
            return read(seq.rptr);
        }

        constexpr view::rvec read_word_chars() {
            auto seq = remain();
            while (number::is_alnum(seq.current()) || seq.current() == '-') {
                seq.consume();
            }
            return read(seq.rptr);
        }

        constexpr view::rvec read_uri_chars() {
            auto seq = remain();
            auto is_uri = [](auto C) {
                return number::is_alnum(C) ||
                       C == '-' || C == '#' || C == ';' || C == '/' ||
                       C == '?' || C == ':' || C == '@' || C == '&' ||
                       C == '=' || C == '+' || C == '$' || C == ',' ||
                       C == '_' || C == '.' || C == '!' || C == '~' ||
                       C == '*' || C == '\'' || C == '(' || C == ')' ||
                       C == '[' || C == ']';
            };
            while (is_uri(seq.current())) {
                seq.consume();
            }
            return read(seq.rptr);
        }

        constexpr view::rvec read_comment_text() {
            const auto begin = r.offset();
            if (expect("#").null()) {
                return {};
            }
            read_nb_chars();  // optional
            return r.read().substr(begin);
        }

        constexpr view::rvec read_comment_break() {
            if (auto v = read_line_break(); !v.null()) {
                return v;
            }
            if (r.empty()) {
                return read(0);
            }
            return {};
        }

        constexpr view::rvec read_in_scalar_comment() {
            const auto begin = r.offset();
            if (!read_separate_in_line().null()) {
                read_comment_text();
            }
            if (read_comment_break().null()) {
                return {};
            }
            return r.read().substr(begin);
        }

        constexpr view::rvec read_outof_scalar_comment() {
            const auto begin = r.offset();
            if (read_separate_in_line().null()) {
                return {};
            }
            read_comment_text();
            if (read_comment_break().null()) {
                r.reset(begin);
                return {};
            }
            return r.read().substr(begin);
        }

        constexpr view::rvec read_comments() {
            const auto begin = r.offset();
            if (read_in_scalar_comment().null()) {
                if (!is_start_of_line()) {
                    return {};
                }
            }
            while (!read_outof_scalar_comment().null()) {
                // repeat
            }
            return r.read().substr(begin);
        }

        constexpr view::rvec read_separate_lines(size_t n) {
            const auto begin = r.offset();
            if (auto v = read_comments(); !v.null()) {
                if (auto v = read_flow_line_prefix(n); !v.null()) {
                    return r.read().substr(begin);
                }
                r.reset(begin);
            }
            return read_separate_in_line();
        }

        constexpr view::rvec read_separate(size_t n, Place c) {
            switch (c) {
                case Place::block_in:
                case Place::block_out:
                case Place::flow_in:
                case Place::flow_out:
                    return read_separate_lines(n);
                case Place::block_key:
                case Place::flow_key:
                    return read_separate_in_line();
                default:
                    return {};
            }
        }
    };

    namespace test {
        constexpr bool check_basic_lexer() {
            constexpr auto data = view::cstr_to_bytes(
                R"(# Comment
                        # lines
                value)");
            constexpr auto expect = view::cstr_to_bytes(R"(# Comment
                        # lines
)");
            io::reader r{view::rvec(data)};
            BasicLexer b{r};
            auto result = b.read_comments();
            return result == expect;
        }

        static_assert(check_basic_lexer());
    }  // namespace test

}  // namespace utils::yaml::lexer
