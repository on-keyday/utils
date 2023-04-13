/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include "parser.h"
#include <minilang/comb/make_json.h>
#include <minilang/comb/print.h>
#include <file/file_view.h>
#include <space/line_pos.h>
#include <json/json_export.h>
#include "binp.h"

struct Flags : utils::cmdline::templ::HelpOption {
    std::string input;
    bool parse_js = false;

    void bind(utils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString(&input, "i,input", "input file", "FILE");
        ctx.VarBool(&parse_js, "print-json", "print json");
    }
};

auto& cout = utils::wrap::cout_wrap();
auto& cerr = utils::wrap::cerr_wrap();

int Main(Flags& flags, utils::cmdline::option::Context& ctx) {
    utils::file::View view;
    if (!view.open(flags.input)) {
        cerr << "binp: error: failed to open file " << flags.input << "\n";
        return -1;
    }
    auto seq = utils::make_ref_seq(view);
    binp::parser::PrintCtx<decltype(cout)> p{cout};
    p.br.discard_not_match = true;
    p.set = binp::parser::print_none;
    std::string out;
    cerr << "parser size: " << sizeof(binp::parser::parse) << "\n";
    auto print_err = [&](auto&& msg, binp::parser::Pos pos, bool as_note = false) {
        seq.rptr = pos.begin;
        cerr << (as_note ? "note: " : "binp: error: ") << msg << "\n";
        out.clear();
        auto loc = utils::space::write_src_loc(out, seq, pos.end - pos.begin);
        cerr << flags.input << ":" << loc.line + 1 << ":" << loc.pos + 1 << "\n";
        cerr << out << "\n";
    };
    if (binp::parser::parse(seq, p) != binp::parser::CombStatus::match) {
        print_err("failed to parse", binp::parser::Pos{seq.rptr, seq.rptr + 1});
        return -1;
    }
    p.br.iterate(binp::parser::branch::make_json(out));
    out.pop_back();
    auto node = utils::json::parse<utils::json::JSON>(out);
    if (node.is_undef()) {
        cerr << "binp: error: unstructured data. BUG!!\n";
        cerr << out;
        return -1;
    }
    if (flags.parse_js) {
        auto js = utils::json::parse<utils::json::OrderedJSON>(out);
        auto str = utils::json::to_string<std::string>(js, utils::json::FmtFlag::no_line | utils::json::FmtFlag::unescape_slash | utils::json::FmtFlag::last_line);
        cout << str;
    }
    binp::Program prog;
    if (!binp::traverse(prog, node)) {
        cerr << "binp: error: traverse error. BUG!!\n";
        return -1;
    }
    binp::Env env;
    binp::generate(env, prog);
    cout << env.out.out();
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return utils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str) { cout << str; },
        [](Flags& flags, utils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
