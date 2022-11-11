/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// compile - bnf to program
#pragma once
#include "../helper/indent.h"

namespace utils {
    namespace bnf {
        template <class T>
        using IW = helper::IndentWriter<T>;

        template <class T>
        auto if_writer(IW<T>& w, auto&& inner) {
            return [&w, ret](auto&&... op) {
                w.write("if(", op..., ") {");
                w.indent(1);
                inner(w);
                w.indent(-1);
                w.write("}");
                w.write_line();
            };
        }

        template <class T>
        void write_utfparser(auto&& output, IW<T>& w) {
            w.write("int BNF_parse_utf8(const char* input,size_t inlen,unsigned int* output,size_t* parsedlen) {");
            w.indent(1);
            auto on_error = if_writer(w, [](auto& w) { w.write("return 0;"); });
            on_error("!input||!inlen||!output||!parsedlen");
            w.write("unsigned char top = (unsigned char)input[0];");
            if_writer(w, [](auto& w) {
                w.write("*output=top;");
                w.write("*parsedllen=1");
                w.write("return 1");
            })("top <= 0x7f");
            on_error("inlen < 2");
            w.write("unsigned char second = (unsigned char)input[1]");
            if_writer(w, [&](auto& w) {
                on_error("top<0xff");
            })("(top&0xE0)==0xC0&&(second&0xC0)==0x80");
        }
    }  // namespace bnf
}  // namespace utils
