/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "minilang.h"
#include <parser/stream/token_stream.h>
#include <parser/stream/tree_stream.h>
#include <number/char_range.h>
#include <wrap/cout.h>
#include <helper/line_pos.h>

namespace minilang {
    namespace st = utils::parser::stream;

    st::Stream make_stream() {
        auto num = st::make_simplecond<wrap::string>("integer", [](const char* c) {
            return utils::number::is_digit(*c);
        });
        auto ur = st::make_unary<true>(std::move(num), st::Expect{"+"}, st::Expect{"-"}, st::Expect{"!"},
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
            stat.pos = pos;
            stat.trimming_stream = &trim;
            return stat;
        }
    };

    static auto& cout = utils::wrap::cout_wrap();
    template <class T>
    void walk(st::Token& tok, Sequencer<T>& seq) {
        cout << tok.stoken() << "\n";
        wrap::string s;
        seq.rptr = tok.pos();
        helper::write_src_loc(s, seq);
        cout << s << "\n";
        if (is_kind(tok, st::tokenBinary)) {
            walk(as<st::BinaryToken>(tok)->left, seq);
            walk(as<st::BinaryToken>(tok)->right, seq);
        }
        else if (is_kind(tok, st::tokenUnary)) {
            walk(as<st::UnaryToken>(tok)->target, seq);
        }
    }

    void test_code() {
        auto st = make_stream();
        MockInput mock{};
        mock.trim = st::make_char(' ');
        constexpr auto src = "3+5";
        mock.raw = src;
        mock.len = 3;
        auto tok = st.parse(mock);
        if (has_err(tok)) {
            cout << tok.err().serror();
        }
        auto loc = make_ref_seq(src);
        walk(tok, loc);
        constexpr auto bad = "55+";
        mock.raw = bad;
        mock.len = 3;
        tok = st.parse(mock);
        auto uw = tok.err().unwrap();

        cout << uw.serror();
    }
}  // namespace minilang
