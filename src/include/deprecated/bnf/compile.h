/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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
            return [&w, inner](auto&&... op) {
                w.write_line("if (", op..., ") {");
                w.indent(1);
                inner(w);
                w.indent(-1);
                w.write_line("}");
                w.write_ln();
            };
        }

        constexpr auto utf8parser_writer(auto&&... funcname) {
            return [=](auto& w, const char* prefix = nullptr, const char* char_type = "char") {
                if (prefix) {
                    w.write_raw(prefix, " ");
                }
                w.write_indent();
                w.write_raw("int ", funcname..., "(const ", char_type, "* input,size_t inlen,unsigned int* output,size_t* parsedlen) {");
                w.write_line();
                w.indent(1);
                auto on_error = if_writer(w, [](auto& w) { w.write("return 0;"); });
                on_error("!input||!inlen||!output||!parsedlen");
                w.write("unsigned char top = (unsigned char)input[0];");
                if_writer(w, [](auto& w) {
                    w.write("*output=top;");
                    w.write("*parsedlen=1;");
                    w.write("return 1;");
                })("top <= 0x7f");
                on_error("inlen < 2");
                w.write("unsigned char second = (unsigned char)input[1];");
                if_writer(w, [&](auto& w) {
                    w.write("*parsedlen=2;");
                    on_error("top<0xC2||0xDf<top||second<0x80||0xBF<second");
                    w.write("*output=0;");
                    w.write("*output|=(unsigned int)(top&0x3f)<<6;");
                    w.write("*output|=(unsigned int)(second&0x3f);");
                    w.write("return 1;");
                })("(top&0xE0)==0xC0");
                on_error("inlen < 3");
                w.write("unsigned char third = (unsigned char)input[2];");
                if_writer(w, [&](auto& w) {
                    w.write("*parsedlen=3;");
                    on_error("top<0xE0||0xEF<top");
                    on_error("top==0xE0&&(second<0xA0||0xBF<second)");
                    on_error("top==0xE4&&(second<0x80||0x9F<second)");
                    on_error("top!=0xE0&&top!=0xE4&&(second<0x80||0xBF<second)");
                    w.write("*output=0;");
                    w.write("*output|=(unsigned int)(top&0x1F)<<12;");
                    w.write("*output|=(unsigned int)(second&0x3F)<<6;");
                    w.write("*output|=(unsigned int)(third&0x3F);");
                    w.write("return 1;");
                })("(top&0xF0)==0xE0");
                on_error("inlen < 4");
                w.write("unsigned char forth = (unsigned char)input[3];");
                if_writer(w, [&](auto& w) {
                    w.write("*parsedlen=4;");
                    on_error("top<0xF0||0xF4<top");
                    on_error("top==0xF0&&(second<0x90||0xBF<second)");
                    on_error("top==0xF4&&(second<0x80||0x8F<second)");
                    on_error("top!=0xF0&&top!=0xF4&&(second<0x80||0xBF<second)");
                    w.write("*output=0;");
                    w.write("*output|=(unsigned int)(top&0x0F)<<18;");
                    w.write("*output|=(unsigned int)(second&0x3F)<<12;");
                    w.write("*output|=(unsigned int)(third&0x3F)<<6;");
                    w.write("*output|=(unsigned int)(forth&0x3F);");
                    w.write("return 1;");
                })("(top&0xF8)==0xF0");
                w.write("return 0;");
                w.indent(-1);
                w.write("}");
            };
        }

        constexpr auto utf_writer(auto&& funcname) {
            return [=](auto& w) {
                utf8parser_writer(funcname, "_utf8parse")(w, "static");
                w.write("int ", funcname, "(const char", ")");
            };
        }
    }  // namespace bnf
}  // namespace utils
