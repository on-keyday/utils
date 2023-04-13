/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../escape/read_string.h"
#include <string>
#include "comb.h"
#include "nullctx.h"

namespace utils::minilang::comb::known {

    // for example: R"a(hello)a"
    //    quote_begin = R"
    //    quote_end = "
    //    cpp_style_begin = (
    //    cpp_style_end = )
    // for example `"hello"`
    //    quote_begin = quote_end = `
    // for example <<EOS
    //               text
    //               EOS
    //    quote_begin = <<
    //    quote_end = nullptr
    //    multiline = true
    constexpr auto make_string_parser(
        const char* quote_begin = "\"",
        const char* quote_end = "\"",
        bool escape_seq = true,
        bool multiline = false,
        const char* cpp_style_begin = nullptr,
        const char* cpp_style_end = nullptr,
        bool cpp_style_multiline = false) {
        const auto has_cpp_style = cpp_style_begin && cpp_style_end;         // C++ raw string style
        const auto has_heardoc = !has_cpp_style && !quote_end && multiline;  // bash hear document style
        return [=](auto& seq, auto&& ctx, auto&&) {
            if (!seq.seek_if(quote_begin)) {
                return CombStatus::not_match;
            }
            auto is_line = [&](auto c) {
                return c == '\r' || c == '\n';
            };
            auto match_eol = [&]() {
                return seq.seek_if("\r\n") || seq.seek_if("\r") || seq.seek_if("\n");
            };
            Pos range{0, 0};
            if (has_cpp_style) {  // C++ raw string style
                range.begin = seq.rptr;
                while (true) {
                    range.end = seq.rptr;
                    if (seq.seek_if(cpp_style_begin)) {
                        break;
                    }
                    if (seq.eos()) {
                        report_error(ctx, "unexpected EOF at reading C++ raw string style prefix");
                        return CombStatus::fatal;
                    }
                    if (!cpp_style_multiline && is_line(seq.current())) {
                        report_error(ctx, "unexpected line at reading C++ raw string style prefix");
                        return CombStatus::fatal;
                    }
                    seq.consume();
                }
            }
            else if (has_heardoc) {  // bash hear document style
                range.begin = seq.rptr;
                while (true) {
                    range.end = seq.rptr;
                    if (match_eol()) {
                        break;
                    }
                    if (seq.eos()) {
                        return CombStatus::fatal;
                    }
                    seq.consume();
                }
            }
            auto match_range = [&] {
                auto ptr = seq.rptr;
                for (auto i = range.begin; i < range.end; i++) {
                    if (seq.eos() || seq.current() != seq.buf.buffer[i]) {
                        seq.rptr = ptr;
                        return false;
                    }
                    seq.consume();
                }
                return true;
            };
            auto match_end = [&] {
                auto ptr = seq.rptr;
                if (has_heardoc) {  // heardoc mode
                    if (!match_eol()) {
                        return false;
                    }
                    if (!match_range()) {
                        seq.rptr = ptr;
                        return false;
                    }
                    if (!match_eol() && !seq.eos()) {
                        seq.rptr = ptr;
                        return false;
                    }
                    return true;
                }
                if (has_cpp_style) {
                    if (!seq.seek_if(cpp_style_end)) {
                        return false;
                    }
                    if (!match_range()) {
                        seq.rptr = ptr;
                        return false;
                    }
                }
                if (!seq.seek_if(quote_end)) {
                    seq.rptr = ptr;
                    return false;
                }
                return true;
            };
            // main loop
            while (true) {
                if (seq.eos()) {
                    report_error(ctx, "unexpected EOF at reading string");
                    return CombStatus::fatal;
                }
                if (match_end()) {
                    break;
                }
                if (!multiline && is_line(seq.current())) {
                    report_error(ctx, "unexpected line at reading string");
                    return CombStatus::fatal;
                }
                if (escape_seq) {
                    if (seq.current() == '\\') {
                        seq.consume();
                        if (seq.eos()) {
                            report_error(ctx, "unexpected EOF at reading escape sequence");
                            return CombStatus::fatal;
                        }
                        if (!multiline && is_line(seq.current())) {
                            report_error(ctx, "unexpected line at reading string");
                            return CombStatus::fatal;
                        }
                    }
                }
                seq.consume();
            }
            return CombStatus::match;
        };
    }

    constexpr auto c_string = make_string_parser();
    constexpr auto js_string = make_string_parser("'", "'");
    constexpr auto go_multi_string = make_string_parser("`", "`", false, true);
    constexpr auto bash_heardoc = make_string_parser("<<", nullptr, false, true);
    constexpr auto python_multi_string = make_string_parser("'''", "'''", false, true);
    constexpr auto cpp_raw_string = make_string_parser("R\"", "\"", false, true, "(", ")");

    namespace test {
        constexpr bool check_string() {
            auto do_test = [](const char* text, auto fn, CombStatus req = CombStatus::match) {
                auto seq = make_ref_seq(text);
                if (fn(seq, 0, 0) != req) {
                    return false;
                }
                return true;
            };
            auto no_escape = make_string_parser("\"", "\"", false);
            return do_test(R"("hello\"\r\n\x20\\world")", c_string) &&
                   do_test(R"('do\'ok"')", js_string) &&
                   do_test(R"(<<EOS
                   hello world
EOS)",
                           bash_heardoc) &&
                   do_test(R"('''''')", python_multi_string) &&
                   do_test(R"abc(R"hoge(hello world!)hoge")abc", cpp_raw_string) &&
                   do_test(R"(`text 
                   literal`)",
                           go_multi_string) &&
                   do_test(R"("\")", c_string, CombStatus::fatal) &&
                   do_test(R"("\")", no_escape);
        }

        static_assert(check_string());
    }  // namespace test
}  // namespace utils::minilang::comb::known
