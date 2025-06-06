/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "minilang.h"
#include <deprecated/stream/token_stream.h>
#include <deprecated/stream/tree_stream.h>
#include <number/char_range.h>
#include <wrap/cout.h>
#include <space/line_pos.h>
#include <unicode/utf/convert.h>
#include <deprecated/stream/utf_stream.h>
#include <deprecated/stream/semantics_stream.h>
#include <deprecated/stream/string_stream.h>

namespace minilang {
    namespace st = futils::parser::stream;

    struct Identifier {
        bool ok(st::UTFStat* stat, auto& str) {
            if (stat->index == 0) {
                return !number::is_symbol_char(stat->C) && !number::is_digit(stat->C);
            }
            if (number::is_symbol_char(stat->C)) {
                return false;
            }
            return true;
        }
        st::ErrorToken endok(auto& str) {
            if (!str.size()) {
                return st::simpleErrToken{"expect identifier but not"};
            }
            return {};
        }
    };

    st::Stream make_stream() {
        auto num = st::make_number<wrap::string>([](auto C) {
            return !number::is_symbol_char(C) && !number::is_digit(C);
        });

        auto trim = st::make_bothtrimming(std::move(num));
        auto ur = st::make_unary<true>(std::move(trim), st::Expect{"+"}, st::Expect{"-"}, st::Expect{"!"},
                                       st::Expect{"*"}, st::Expect{"&"});
        auto ok_eq = [](st::Input& input) { return !input.expect("="); };
        auto ok_div = [](st::Input& input) { return !input.expect("*") && !input.expect("="); };
        auto ok_band = [](st::Input& input) { return !input.expect("&") && !input.expect("="); };
        auto ok_bor = [](st::Input& input) { return !input.expect("|") && !input.expect("="); };
        auto mul = st::make_binary(
            std::move(ur), st::ExpectWith{"*", ok_eq}, st::ExpectWith{"/", ok_eq}, st::ExpectWith{"%", ok_div}, st::ExpectWith{"&", ok_band},
            st::ExpectWith{">>", ok_eq}, st::ExpectWith{"<<", ok_eq});
        auto add = st::make_binary(
            std::move(mul), st::ExpectWith{"+", ok_eq}, st::ExpectWith{"-", ok_eq},
            st::ExpectWith{"|", ok_bor});
        auto equal = st::make_binary(std::move(add),
                                     st::Expect{"=="}, st::Expect{"!="}, st::Expect{">="}, st::Expect{"<="},
                                     st::Expect{"<"}, st::Expect{">"});
        auto assign = st::make_assign(std::move(equal), st::Expect{"="}, st::Expect{"+="},
                                      st::Expect{"-="}, st::Expect{"*="}, st::Expect{"/="}, st::Expect{"%="});

        return assign;
    }

    struct MockInput {
        const char* raw;
        size_t len;
        size_t pos;
        st::Stream trim;

        const char* peek(char*, size_t, size_t* r) {
            *r = len - pos;
            return raw + pos;
        }

        bool seek(size_t p) {
            if (len < pos) {
                return false;
            }
            pos = p;
            return true;
        }

        st::InputStat info() {
            st::InputStat stat{};
            stat.eos = len == pos;
            stat.remain = len - pos;
            stat.sized = true;
            stat.err = false;
            stat.trimming_stream = &trim;
            stat.pos = pos;
            return stat;
        }
    };

    static auto& cout = futils::wrap::cout_wrap();
    template <class T>
    void walk(st::Token& tok, Sequencer<T>& seq) {
        cout << tok.stoken() << "\n";
        wrap::string s;
        seq.rptr = tok.pos();
        space::write_src_loc(s, seq);
        cout << s << "\n";
        if (is_kind(tok, st::tokenBinary)) {
            walk(as<st::BinaryToken>(tok)->left, seq);
            walk(as<st::BinaryToken>(tok)->right, seq);
        }
        else if (is_kind(tok, st::tokenUnary)) {
            walk(as<st::UnaryToken>(tok)->target, seq);
        }
        else if (is_kind(tok, st::tokenNumber)) {
            auto num = as<st::Number<wrap::string>>(tok);
        }
        else if (is_kind(tok, st::tokenTrimed)) {
            walk(as<st::TrimedToken>(tok)->tok, seq);
        }
    }

    void test_code() {
        auto st = make_stream();
        MockInput mock{};
        mock.trim = st::make_char(' ', true);
        constexpr auto src = "3+5";
        mock.raw = src;
        mock.len = 3;
        auto tok = st.parse(mock);
        if (has_err(tok)) {
            cout << tok.err().serror() << "\n";
        }
        auto loc = make_ref_seq(src);
        walk(tok, loc);
        constexpr auto bad = "55+";
        mock.raw = bad;
        mock.len = 3;
        tok = st.parse(mock);
        auto uw = tok.err().unwrap();

        cout << uw.serror();
        st = st::make_utf<wrap::string>(
            "identifier",
            st::RecPrevChecker{
                [](char32_t c, size_t i, char32_t prev) {
                    if (i == 0) {
                        return c == U'定';
                    }
                    else {
                        return prev != U'義';
                    }
                }});

        constexpr auto utftest = u8"定tell義 型string名 ";
        mock.raw = (const char*)utftest;
        mock.len = 24;
        mock.pos = 0;
        tok = st.parse(mock);
        assert(!has_err(tok));
        auto seq = make_ref_seq(utftest);
        walk(tok, seq);

        st = st::make_default_string<wrap::string>(U"\"", U"\"");
        constexpr auto stringtest = R"("test\"\n\u2342\x73\0")";
        mock.raw = stringtest;
        mock.len = futils::strlen(stringtest);
        mock.pos = 0;
        tok = st.parse(mock);
        assert(!has_err(tok));
        loc = make_ref_seq(stringtest);
        walk(tok, loc);
    }
}  // namespace minilang
