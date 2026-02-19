/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include "script.h"
#include <file/file_view.h>
#include <comb2/tree/branch_table.h>
#include "compile.h"
#include <space/line_pos.h>
#include "runtime.h"

struct Flags : futils::cmdline::templ::HelpOption {
    std::string script;
    bool show_tree = true;
    bool show_istrs = true;
    void bind(futils::cmdline::option::Context &ctx) {
        bind_help(ctx);
        ctx.VarString(&script, "s,script", "qurl script file", "FILE");
        ctx.VarBool(&show_tree, "t,show-tree", "show script syntax tree");
        ctx.VarBool(&show_istrs, "i,show-istrs", "show asm-like instructions");
    }
};
auto &cerr = futils::wrap::cerr_wrap();

void eprint(auto &&...args) {
    cerr << "qurl: error: ";
    (cerr << ... << args);
}

void print(auto &&...args) {
    cerr << "qurl: ";
    (cerr << ... << args);
}

void print_errors(auto &&seq, std::vector<qurl::Error> &errs, const std::string &file) {
    for (auto &err : errs) {
        std::string src;
        if (!err.pos.npos()) {
            seq.rptr = err.pos.begin;
            auto len = err.pos.len();
            auto loc = futils::space::write_src_loc(src, seq, err.pos.len(), 10);
            src = std::format("{}:{}:{}:\n{}", file, loc.line + 1, loc.pos + 1, src);
        }
        eprint(err.msg, "\n", src, "\n");
    }
}

struct Context : qurl::comb2::tree::BranchTable {
    void error(auto &&...args) {
        eprint(args..., "\n");
    }
};

template <class T>
bool runCompile(futils::Sequencer<T> &seq, qurl::compile::Env &env, bool show_tree, bool show_istrs, const std::string &file) {
    Context table;
    auto s = qurl::script::parse(seq, table);
    if (s != qurl::comb2::Status::match) {
        std::string src;
        auto loc = futils::space::write_src_loc(src, seq);
        cerr << std::format("{}:{}:{}:\n{}", file, loc.line + 1, loc.pos + 1, src);
        ;
        return false;
    }
    if (show_tree) {
        qurl::compile::CodeW w{"  "};
        qurl::compile::write_nodes(w, table.root_branch);
        print("show tree...\n", w.out());
    }
    auto collected = qurl::compile::node::collect(table.root_branch);
    if (!qurl::compile::compile(env, collected)) {
        print_errors(seq, env.errors, file);
        return false;
    }
    if (show_istrs) {
        qurl::compile::CodeW w{"    "};
        qurl::compile::write_asm(w, env.istrs);
        print("show instructions...\n", w.out());
    }
    return true;
}

void setupBuiltin(qurl::runtime::Env &renv) {
    renv.builtins["__builtin_print"].fn = [](std::shared_ptr<void> &, qurl::runtime::BuiltinAccess ac, qurl::runtime::Value &ret, std::vector<qurl::runtime::Value> &arg) {
        return qurl::runtime::Status::ok;
    };
}

int runScript(auto &&seq, qurl::compile::Env &env, const std::string &file) {
    qurl::runtime::Env renv;
    setupBuiltin(renv);
    renv.istrs = std::make_shared<qurl::runtime::InstrScope>();
    renv.istrs->istrs = &env.istrs;
    while (true) {
        if (auto s = renv.run(); s != qurl::runtime::Status::ok) {
            print_errors(seq, renv.errors, file);
            break;
        }
    }
    return 0;
}

int scriptMain(Flags &flags, futils::cmdline::option::Context &ctx) {
    futils::file::View file;
    if (!file.open(flags.script)) {
        eprint("failed to open file: ", flags.script);
        return -1;
    }
    auto seq = futils::make_ref_seq(file);
    qurl::compile::Env env;
    if (!runCompile(seq, env, flags.show_tree, flags.show_istrs, flags.script)) {
        return 1;
    }
    return runScript(seq, env, flags.script);
}

int Main(Flags &flags, futils::cmdline::option::Context &ctx) {
    if (flags.script.size()) {
        return scriptMain(flags, ctx);
    }
    return 0;
}

int main(int argc, char **argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto &&str) { cerr << str; },
        [](Flags &flags, futils::cmdline::option::Context &ctx) {
            return Main(flags, ctx);
        });
}
