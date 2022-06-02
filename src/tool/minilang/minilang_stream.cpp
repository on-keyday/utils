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

namespace minilang {
    namespace st = utils::parser::stream;

    st::Stream make_stream() {
        auto num = st::make_simplecond<wrap::string>("number", [](const char* c) {
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
            st::InputStat stat;
            stat.eos = len == pos;
            stat.remain = len - pos;
            stat.sized = true;
            stat.err = false;
            stat.pos = pos;
            return stat;
        }
    };

    void test_code() {
        auto st = make_stream();

        constexpr auto src = "3+5";
        auto tok = st.parse(MockInput{src, 3});
        constexpr auto bad = "55+";
        tok = st.parse(MockInput{bad, 3});
        auto uw = tok.err().unwrap();
        auto& cout = utils::wrap::cout_wrap();
        cout << uw.serror();
    }
}  // namespace minilang
