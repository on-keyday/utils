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
#include <wrap/argv.h>
#include <wrap/cout.h>
#include <file/file_view.h>
#include "igen2.h"
namespace option = utils::cmdline::option;
namespace bnf = utils::bnf;
namespace templ = utils::cmdline::templ;
auto& cout = utils::wrap::cout_wrap();

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
