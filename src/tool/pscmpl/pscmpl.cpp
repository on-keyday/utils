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
#include <parser/defs/jsoncvt.h>
#include <json/json_export.h>
#include "pscmpl.h"
#include <wrap/light/smart_ptr.h>
#include <fstream>

auto& cout = utils::wrap::cout_wrap();
auto& cerr = utils::wrap::cerr_wrap();

int main(int argc, char** argv) {
    using namespace utils::cmdline;
    option::Context ctx;
    auto input = ctx.String<utils::wrap::string>("input,i", "", "input file", "FILE", option::CustomFlag::required | option::CustomFlag::appear_once);
    auto output = ctx.String<utils::wrap::string>("output,o", "", "output file", "FILE", option::CustomFlag::required | option::CustomFlag::appear_once);
    bool verbose = false, help = false;
    ctx.VarBool(&verbose, "verbose,v", "verbose log");
    ctx.VarBool(&help, "help,h", "show help");
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
        utils::number::Array<400, char, true> buf;
        ctx.Usage(buf, option::ParseFlag::assignable_mode, argv[0]);
        cout << buf;
        return 1;
    }
    utils::file::View view;
    if (!view.open(*input)) {
        return error("failed to open file ", *input);
    }
    auto seq = utils::make_ref_seq(view);
    utils::wrap::unique_ptr<expr::PlaceHolder> deffer;
    expr::PlaceHolder* ph;
    auto parse = pscmpl::define_parser(seq, ph);
    deffer.reset(ph);
    expr::Expr* expr;
    auto print_json = [](auto&& obj) {
        cout << utils::json::to_string<utils::wrap::string>(
                    utils::json::convert_to_json<utils::json::OrderedJSON>(obj),
                    utils::json::FmtFlag::unescape_slash)
             << "\n";
    };
    auto write_loc = [&]() {
        utils::wrap::string loc;
        utils::helper::write_src_loc(loc, seq);
        cerr << loc << "\n";
    };
    if (!parse(seq, expr)) {
        error("failed to parse file ", *input);
        cerr << "failed location\n";
        write_loc();
        return -1;
    }
    if (verbose) {
        print_json(expr);
    }
    pscmpl::CompileContext cc;
    if (!pscmpl::compile(expr, cc)) {
        error(cc.err);
        cerr << "failed locations\n";
        for (auto err : cc.errstack) {
            utils::wrap::string key;
            err->stringify(key);
            cerr << "object:" << key << "\n";
            cerr << "type:" << err->type() << "\n";
            seq.rptr = err->pos();
            write_loc();
        }
        return -1;
    }
    view.close();
    if (verbose) {
        cout << cc.buffer;
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
