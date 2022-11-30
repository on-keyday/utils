/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <bnf/bnf.h>
#include <bnf/matching.h>
#include <bnf/compile.h>
#include <wrap/argv.h>
#include <wrap/cout.h>
#include <file/file_view.h>
#include "igen2.h"
namespace option = utils::cmdline::option;
namespace bnf = utils::bnf;
namespace templ = utils::cmdline::templ;
auto& cout = utils::wrap::cout_wrap();

constexpr int parse_utf8(const char8_t* input, size_t inlen, unsigned int* output, size_t* parsedlen) {
    if (!input || !inlen || !output || !parsedlen) {
        return 0;
    }

    unsigned char top = (unsigned char)input[0];
    if (top <= 0x7f) {
        *output = top;
        *parsedlen = 1;
        return 1;
    }

    if (inlen < 2) {
        return 0;
    }

    unsigned char second = (unsigned char)input[1];
    if ((top & 0xE0) == 0xC0) {
        if (top < 0xC2 || 0xDf < top || second < 0x80 || 0xBF < second) {
            return 0;
        }

        *output = 0;
        *output |= (unsigned int)(top & 0x3f) << 6;
        *output |= (unsigned int)(second & 0x3f);
        *parsedlen = 2;
        return 1;
    }

    if (inlen < 3) {
        return 0;
    }

    unsigned char third = (unsigned char)input[2];
    if ((top & 0xF0) == 0xE0) {
        if (top < 0xE0 || 0xEF < top) {
            return 0;
        }

        if (top == 0xE0 && (second < 0xA0 || 0xBF < second)) {
            return 0;
        }

        if (top == 0xE4 && (second < 0x80 || 0x9F < second)) {
            return 0;
        }

        if (top != 0xE0 && top != 0xE4 && (second < 0x80 || 0xBF < second)) {
            return 0;
        }

        *output = 0;
        *output |= (unsigned int)(top & 0x1F) << 12;
        *output |= (unsigned int)(second & 0x3F) << 6;
        *output |= (unsigned int)(third & 0x3F);
        *parsedlen = 3;
        return 1;
    }

    if (inlen < 4) {
        return 0;
    }

    unsigned char forth = (unsigned char)input[3];
    if ((top & 0xF8) == 0xF0) {
        if (top < 0xF0 || 0xF4 < top) {
            return 0;
        }

        if (top == 0xF0 && (second < 0x90 || 0xBF < second)) {
            return 0;
        }

        if (top == 0xF4 && (second < 0x80 || 0x8F < second)) {
            return 0;
        }

        if (top != 0xF0 && top != 0xF4 && (second < 0x80 || 0xBF < second)) {
            return 0;
        }

        *output = 0;
        *output |= (unsigned int)(top & 0x0F) << 18;
        *output |= (unsigned int)(second & 0x3F) << 12;
        *output |= (unsigned int)(third & 0x3F) << 6;
        *output |= (unsigned int)(forth & 0x3F);
        *parsedlen = 4;
        return 1;
    }

    return 0;
}

constexpr char32_t parse_literal(const char8_t* input, size_t* len) {
    if (!input || !len) {
        return false;
    }
    unsigned int output;
    size_t parsed = 0;
    if (!parse_utf8(input, *len, &output, &parsed)) {
        char buf[] = "expect 0 byte utf string. but invalid byte detected.";
        buf[7] = '0' + parsed;
    }
    return output;
}

struct Option : templ::HelpOption {
    std::string input, deffile;
    void bind(option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString(&input, "input,i", "input file", "FILE");
        // ctx.VarString(&deffile, "define,d", "definition file", "FILE", option::CustomFlag::required);
    }
};

bool parse_test() {
    auto seq = utils::make_ref_seq(R"(
            RULE:=A B (C | D) EOF
            A:="A"
            B:="B"
            C:="C"
            D:= ("D"| %x64 %x83-84 ) # comment
            E:= 3D
        )");
    igen2::ErrC errc(&seq);
    std::map<std::string, std::shared_ptr<bnf::BnfDef>> bnfs;
    if (!bnf::parse(seq, errc, [&](std::shared_ptr<bnf::BnfDef> def) {
            bnfs[def->rule_name()] = std::move(def);
            return true;
        })) {
        return false;
    }
    for (auto& bnf : bnfs) {
        if (!bnf::resolve(bnf.second->bnf, errc, [&](auto& name) {
                return bnfs[name];
            })) {
            return false;
        }
    }
    auto rule = bnfs["RULE"];
    std::vector<bnf::StackFrame> stack;
    auto test = utils::make_ref_seq("ABD");
    errc.buffer_mode = true;
    auto stack_trace = [&](const std::vector<bnf::StackFrame>& stack, bool enter, bnf::StackState s) {
        cout << (enter ? "enter" : "leave")
             << " stack: " << stack.back().bnf->node->str << " "
             << (s != bnf::StackState::failure ? "ok" : "failed") << "\n";
    };
    if (!bnf::matching(stack, rule->bnf, test, errc, stack_trace)) {
        cout << errc.pack.pack();
        return false;
    }
    bnf::IW<std::string> val("", "    ");
    bnf::utf8parser_writer("parse_utf8")(val, "constexpr", "char8_t");
    cout << val.t;
    return true;
}

int run_main(Option& opt, option::Context& c) {
    if (!parse_test()) {
        return -1;
    }
    utils::file::View view;
    if (!view.open(opt.deffile)) {
        cout << "failed to open file " << opt.deffile << "\n";
        return -1;
    }
    return 0;
}

int main(int argc, char** argv) {
    utils::wrap::U8Arg _(argc, argv);
    Option opt;
    auto show = [&](auto&& msg) {
        cout << msg;
    };
    return templ::parse_or_err<std::string>(argc, argv, opt, show, run_main);
}
