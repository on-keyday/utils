/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include "parser.h"
#include <file/file_view.h>
#include <json/json_export.h>
#include <json/to_string.h>
#include <space/line_pos.h>
#include <number/to_string.h>
#include "traverse.h"
#include "ipret.h"
#include "compile.h"

namespace json = futils::json;
struct Flags : futils::cmdline::templ::HelpOption {
    std::string input;
    bool parse_js = false;
    json::FmtFlag fmt_html = {};
    bool escape_more = false;
    std::string mode = "ipret";
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString(&input, "i,input", "input file", "FILE", futils::cmdline::option::CustomFlag::required);
        ctx.VarBool(&parse_js, "j,print-json", "print json");
        ctx.VarFlagSet(&fmt_html, "f,format-html", json::FmtFlag::html, "format json with html escape");
        ctx.VarBool(&escape_more, "e,escape-more", "escape json as string literal");
        ctx.VarString(&mode, "mode", "runner mode. ipret,compile,none. default: ipret", "MODE");
    }
};
auto& cout = futils::wrap::cout_wrap();
auto& cerr = futils::wrap::cerr_wrap();

namespace br = futils::minilang::comb::branch;

int run_interpreter(std::shared_ptr<combl::trvs::Scope>& global_scope, auto&& print_err) {
    combl::ipret::InEnv env;
    env.global_scope = global_scope;
    env.builtin_funcs["__builtin_suspend"].eval_builtin = [](std::shared_ptr<void>&, combl::istr::Callback& cb, combl::ipret::Value& ret, std::vector<combl::ipret::Value>& args, size_t call_time) {
        if (call_time == 0) {
            return combl::istr::EvalResult::suspend;
        }
        return combl::istr::EvalResult::ok;
    };
    env.builtin_funcs["__builtin_exit"].eval_builtin = [](std::shared_ptr<void>&, combl::istr::Callback& cb, combl::ipret::Value& ret, std::vector<combl::ipret::Value>& args, size_t call_time) {
        return combl::istr::EvalResult::exit;
    };
    while (eval(env)) {
        if (env.call_stack == nullptr) {
            break;
        }
    }
    bool check = false;
    for (auto& err : env.err) {
        print_err(err.reason, err.pos, check);
        check = true;
    }
    return check ? -1 : 0;
}

int run_compiler(std::shared_ptr<combl::trvs::Scope>& global_scope) {
    combl::bin::Compiler cm;
    cm.global_scope = global_scope;
    return combl::bin::compile(cm) ? 0 : -1;
}

int run_main(Flags& flags, futils::cmdline::option::Context& ctx) {
    futils::file::View view;
    if (!view.open(flags.input)) {
        cerr << "combl: error: failed to open file " << flags.input << "\n";
        return -1;
    }
    auto seq = futils::make_ref_seq(view);
    combl::parser::PrintCtx<decltype(cout)> p{cout};
    p.br.discard_not_match = true;
    p.set = combl::parser::print_none;
    std::string out;
    cerr << "parser size: " << sizeof(combl::parser::parse) << "\n";
    auto print_err = [&](auto&& msg, combl::trvs::Pos pos, bool as_note = false) {
        seq.rptr = pos.begin;
        cerr << (as_note ? "note: " : "combl: error: ") << msg << "\n";
        out.clear();
        auto loc = futils::space::write_src_loc(out, seq, pos.end - pos.begin);
        cerr << flags.input << ":" << loc.line + 1 << ":" << loc.pos + 1 << "\n";
        cerr << out << "\n";
    };
    if (combl::parser::parse(seq, p) != combl::parser::CombStatus::match) {
        print_err("failed to parse", combl::trvs::Pos{seq.rptr, seq.rptr + 1});
        return -1;
    }
    p.br.iterate(br::make_json(out));
    out.pop_back();
    auto global_scope = std::make_shared<combl::trvs::Scope>();
    auto node = futils::json::parse<futils::json::JSON>(out);
    if (node.is_undef()) {
        cerr << "combl: error: unstructured data. BUG!!\n";
        cerr << out;
        return -1;
    }
    if (flags.parse_js) {
        auto js = futils::json::parse<futils::json::OrderedJSON>(out);
        auto str = futils::json::to_string<std::string>(js, flags.fmt_html | futils::json::FmtFlag::no_line | futils::json::FmtFlag::unescape_slash | futils::json::FmtFlag::last_line);
        if (flags.escape_more) {
            std::string more;
            futils::escape::escape_str(str, more);
            str = "\'" + std::move(more) + "\'";
        }
        cout << str;
    }
    if (!combl::trvs::traverse(global_scope, node)) {
        cerr << "combl: error: failed to traverse structure. BUG!!";
        return -1;
    }
    if (flags.mode == "compile") {
        return run_compiler(global_scope);
    }
    else if (flags.mode == "ipret") {
        return run_interpreter(global_scope, print_err);
    }
    else if (flags.mode == "none") {
        return 0;
    }
    cerr << "combl: error: unknown runner mode " << flags.mode;
    return -1;
}

int main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str) { cerr << "combl: " << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return run_main(flags, ctx);
        });
}
