/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <langc/c_lex.h>
#include <langc/c_ast.h>
#include <langc/c_x64.h>
#include <code/src_location.h>
#include <escape/escape.h>
#include <file/file_view.h>
struct Flags : utils::cmdline::templ::HelpOption {
    std::vector<std::string_view> args;
    bool print_tokens = false;
    void bind(utils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&print_tokens, "print-tokens", "print tokenized data");
    }
};
auto& cout = utils::wrap::cout_wrap();
auto& cerr = utils::wrap::cerr_wrap();

int Main(Flags& flags, utils::cmdline::option::Context& ctx) {
    if (flags.args.size() == 0) {
        cerr << "error: no input file\n";
        return -1;
    }
    utils::file::View view;
    if (!view.open(flags.args[0])) {
        cerr << "error: failed to open file " << flags.args[0] << "\n";
        return -1;
    }
    auto seq = utils::make_ref_seq(view);
    auto tokens = utils::langc::tokenize(seq);
    auto print_err = [&](utils::langc::Error err) {
        std::string src;
        seq.rptr = err.pos.begin;
        auto loc = utils::code::write_src_loc(src, seq);
        cerr << "error: " << err.err << "\n";
        cerr << flags.args[0] << ":" << loc.line + 1 << ":" << loc.pos + 1 << ":\n";
        cerr << src << "\n";
    };
    if (!tokens) {
        print_err(utils::langc::Error{.err = "expect eof but not", .pos = {seq.rptr, seq.rptr + 1}});
        return -1;
    }
    if (flags.print_tokens) {
        for (auto& token : *tokens) {
            cout << "----------\ntoken: " << utils::escape::escape_str<std::string>(token.token) << "\ntag: " << token.tag << "\npos: {" << token.pos.begin << "," << token.pos.end << "}\n";
        }
    }
    utils::langc::Errors errs;
    auto prog = utils::langc::ast::parse_c(*tokens, errs);
    if (!prog) {
        for (auto& err : errs.errs) {
            print_err(err);
        }
        return -1;
    }
    utils::langc::gen::Writer w;
    if (!utils::langc::gen::x64::gen_prog(w, errs, prog)) {
        for (auto& err : errs.errs) {
            print_err(err);
        }
        return -1;
    }
    cout << w.out();
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return utils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, utils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
