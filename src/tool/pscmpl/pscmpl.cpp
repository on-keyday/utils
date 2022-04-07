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

auto& cout = utils::wrap::cout_wrap();

int main(int argc, char** argv) {
    using namespace utils::cmdline;
    option::Context ctx;
    auto input = ctx.String<utils::wrap::string>("input,i", "", "input file", "FILE", option::CustomFlag::required | option::CustomFlag::appear_once);
    auto output = ctx.String<utils::wrap::string>("output,o", "", "output file", "FILE", option::CustomFlag::required | option::CustomFlag::appear_once);
    auto err = option::parse_required(argc, argv, ctx, utils::helper::nop, option::ParseFlag::assignable_mode);
    auto error = [](auto&&... args) {
        cout << "pscmpl: error: ";
        (cout << ... << args) << "\n";
        return -1;
    };
    if (auto msg = error_msg(err)) {
        return error(ctx.erropt(), ": ", msg);
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
        cout << loc << "\n";
    };
    if (!parse(seq, expr)) {
        error("failed to parse file ", *input);
        cout << "failed location\n";
        write_loc();
        return -1;
    }
    print_json(expr);
    pscmpl::CompileContext cc;
    if (!pscmpl::compile(expr, cc)) {
        error(cc.err);
        cout << "failed locations\n";
        for (auto err : cc.errstack) {
            seq.rptr = err->pos();
            write_loc();
        }
        return -1;
    }
}
