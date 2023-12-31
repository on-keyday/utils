/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "minilang.h"
#include <cmdline/option/optcontext.h>
#include <wrap/cout.h>
#include <file/file_view.h>
#include <space/line_pos.h>
#include <fstream>
using namespace utils::cmdline;
namespace hlp = utils::helper;
namespace wrap = utils::wrap;
auto& cout = utils::wrap::cout_wrap();
auto& cerr = utils::wrap::cerr_wrap();

enum class OutputFormat {
    none,
    llvm,
};

struct CmdLineOption {
    bool test_code = false;
    wrap::string input;
    bool dbginfo = false;
    bool help = false;
    wrap::string output;
    OutputFormat format;
    void bind(option::Context& ctx) {
        ctx.VarString(&input, "input,i", "input file", "FILE", option::CustomFlag::required | option::CustomFlag::appear_once);
        ctx.VarString(&output, "output,o", "output file", "FILE", option::CustomFlag::appear_once);
        ctx.VarBool(&dbginfo, "debug-info,d", "show debug info");
        ctx.VarBool(&help, "help,h", "show this help");
        ctx.VarBool(&test_code, "test,t", "run test code");
        auto parser = option::MappingParser<wrap::string, OutputFormat, wrap::hash_map>{};
        parser.mapping["none"] = OutputFormat::none;
        parser.mapping["llvm"] = OutputFormat::llvm;
        ctx.Option("format,f", &format, std::move(parser), "set output format. if not set run on interpreter mode", "none|llvm");
    }
};

void dump_stack(auto& opt, auto& stack, auto& seq) {
    for (auto& v : stack) {
        cerr << v.msg << "\n";
        seq.rptr = v.node->expr->pos();
        wrap::string err;
        auto linepos = utils::space::write_src_loc(err, seq);
        cout << opt.input << ":" << linepos.line + 1 << ":" << linepos.pos + 1 << "\n";
        cout << err << "\n";
    }
}

int main(int argc, char** argv) {
    CmdLineOption opt;
    option::Context ctx;
    opt.bind(ctx);
    auto perr = option::parse_required(argc, argv, ctx, hlp::nop, option::ParseFlag::assignable_mode);
    auto report = [](auto... err) {
        cerr << "minilang: error: ";
        (cerr << ... << err);
        cerr << "\n";
    };
    if (perfect_parsed(perr) && opt.help) {
        cout << "minilang - minimum programing language\n";
        cout << ctx.Usage<wrap::string>(option::ParseFlag::assignable_mode, argv[0]);
        return 1;
    }
    if (perfect_parsed(perr) && opt.test_code) {
        minilang::test_code();
        return 0;
    }
    if (auto msg = error_msg(perr)) {
        report(ctx.erropt(), ": ", msg);
        return -1;
    }
    minilang::expr::Expr* expr = nullptr;
    utils::file::View view;
    if (!view.open(opt.input)) {
        report("failed to open file ", opt.input);
        return -1;
    }
    auto seq = utils::make_ref_seq(view);
    minilang::expr::Errors<wrap::string, wrap::vector> errs;
    if (!minilang::parse(seq, expr, errs)) {
        report("failed to parse file ", opt.input);
        cerr << "failed locations\n";
        for (auto& v : errs.stack) {
            cerr << "type: " << v.type << "\n";
            cerr << v.msg << "\n";
            if (opt.dbginfo) {
                cout << "log location: " << v.loc << ":" << v.line << "\n";
            }
            wrap::string err;
            seq.rptr = v.end;
            auto linepos = utils::space::write_src_loc(err, seq);
            cout << "src: " << opt.input << ":" << linepos.line + 1 << ":" << linepos.pos + 1 << "\n";
            cout << err << "\n";
        }
        return -1;
    }
    minilang::Scope scope;
    scope.kind = minilang::ScopeKind::global;
    auto node = minilang::convert_to_node(expr, &scope, true);
    if (opt.format == OutputFormat::none) {
        minilang::runtime::Interpreter inpret;
        inpret.add_builtin(
            "__builtin_print", nullptr, [](void*, const char*, minilang::runtime::RuntimeEnv* env) {
                for (auto& arg : env->args) {
                    if (auto str = arg.as_str()) {
                        cout << *str;
                    }
                }
            });
        if (!inpret.eval(node)) {
            report("eval error!");
            dump_stack(opt, inpret.stack, seq);
            return -1;
        }
        if (opt.dbginfo) {
            cout << "eval succeeded\n";
        }
    }
    else if (opt.format == OutputFormat::llvm) {
        minilang::assembly::NodeAnalyzer analyzer;
        minilang::assembly::Context actx;
        if (!analyzer.dump_llvm(node, actx)) {
            report("generate error!");
            dump_stack(opt, actx.stack, seq);
            return -1;
        }
        if (!ctx.is_set("output")) {
            report("output file not set!. use option --output");
            return -1;
        }
        std::ofstream fs(opt.output);
        if (!fs) {
            report("output file ", opt.output, " couldn't open");
            return -1;
        }
        fs << actx.buffer;
        fs.close();
        if (opt.dbginfo) {
            cout << "saved\n";
        }
    }
    else {
        report("unknown format");
        return -1;
    }
    return 0;
}
