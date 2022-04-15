/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cmdline/option/optcontext.h>
#include <wrap/cout.h>
#include <file/file_view.h>
#include <helper/line_pos.h>
#include <parser/expr/jsoncvt.h>
#include <json/json_export.h>
#include "pscmpl.h"
#include <wrap/light/smart_ptr.h>
#include <fstream>

auto& cout = utils::wrap::cout_wrap();
auto& cerr = utils::wrap::cerr_wrap();

utils::Sequencer<utils::file::View&>* seqptr;

void write_loc(size_t sz = 1) {
    utils::wrap::string loc;
    utils::helper::write_src_loc(loc, *seqptr, sz);
    cerr << loc << "\n";
}

namespace pscmpl {
    void parse_msg(const char* msg) {
        if (seqptr) {
            cerr << msg << "\n";
        }
    }

    void verbose_parse(expr::Expr* expr) {
        if (seqptr && expr) {
            utils::wrap::string key;
            expr->stringify(key);
            if (hlp::equal(expr->type(), "string")) {
                utils::wrap::string tmp;
                utils::escape::escape_str(key, tmp);
                key = "\"" + tmp + "\"";
            }
            cerr << "object:" << key << "\n";
            cerr << "type:" << expr->type() << "\n";
            seqptr->rptr = expr->pos();
            if (key.size() == 0) {
                key.push_back(' ');
            }
            write_loc(key.size());
        }
    }
}  // namespace pscmpl

int main(int argc, char** argv) {
    using namespace utils::cmdline;
    pscmpl::CompileContext cc;
    option::Context ctx;
    auto input = ctx.String<utils::wrap::string>("input,i", "", "input file", "FILE", option::CustomFlag::required | option::CustomFlag::appear_once);
    auto output = ctx.String<utils::wrap::string>("output,o", "", "output file", "FILE", option::CustomFlag::required | option::CustomFlag::appear_once);
    bool verbose = false, help = false, parse_verbose = false, tree_verbose = false, dry_run = false;
    ctx.VarBool(&verbose, "print-code,v", "show compiled code before save");
    ctx.VarBool(&parse_verbose, "parse-verbose,p", "show verbose parse log");
    ctx.VarBool(&tree_verbose, "tree-verbose,t", "show verbose parse tree as json");
    ctx.VarBool(&help, "help,h", "show help");
    ctx.VarBool(&dry_run, "dry-run,d", "dry run to compile");
    ctx.VarFlagSet(&cc.flag, "main", pscmpl::CompileFlag::with_main, "generate main()");
    auto err = option::parse_required(argc, argv, ctx, utils::helper::nop, option::ParseFlag::assignable_mode);
    auto error = [](auto&&... args) {
        cerr << "pscmpl: error: ";
        (cerr << ... << args) << "\n";
        return -1;
    };
    if (auto msg = error_msg(err)) {
        return error(ctx.erropt(), ": ", msg);
    }
    if (help) {
        utils::wrap::string buf;
        ctx.Usage(buf, option::ParseFlag::assignable_mode, argv[0]);
        cout << buf;
        return 1;
    }
    utils::file::View view;
    if (!view.open(*input)) {
        return error("failed to open file ", *input);
    }
    auto seq = utils::make_ref_seq(view);
    if (parse_verbose) {
        seqptr = &seq;
    }
    utils::wrap::unique_ptr<expr::PlaceHolder> defer;
    expr::PlaceHolder* ph;
    pscmpl::ProgramState state;
    auto parse = pscmpl::define_parser(seq, ph, state);
    defer.reset(ph);
    expr::Expr* expr;
    auto print_json = [](auto&& obj) {
        cout << utils::json::to_string<utils::wrap::string>(
                    utils::json::convert_to_json<utils::json::OrderedJSON>(obj),
                    utils::json::FmtFlag::unescape_slash)
             << "\n";
    };
    expr::ErrorStack stack;
    if (!parse(seq, expr, stack)) {
        error("failed to parse file ", *input);
        cerr << "failed location\n";
        seqptr = &seq;
        write_loc();
        return -1;
    }
    if (tree_verbose) {
        print_json(expr);
    }
    if (state.package >= 2) {
        error("multiple package declaration");
        cerr << "failed locations\n";
        seqptr = &seq;
        for (auto err : state.locs) {
            pscmpl::verbose_parse(err);
        }
        return -1;
    }

    if (!pscmpl::compile(expr, cc)) {
        error(cc.err);
        seqptr = &seq;
        cerr << "failed locations\n";
        for (auto err : cc.errstack) {
            pscmpl::verbose_parse(err);
        }
        return -1;
    }
    view.close();
    if (verbose) {
        cout << cc.buffer;
    }
    if (dry_run) {
        cout << "no error\n";
        return 0;
    }
    {
        std::ofstream fs(*output);
        if (!fs) {
            return error("failed to open output file ", *output);
        }
        fs << cc.buffer;
    }
    cout << "generated\n";
}
