/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <parser/defs/command_expr.h>
#include <parser/defs/jsoncvt.h>
#include <wrap/light/string.h>
#include <wrap/light/vector.h>
#include <cmdline/option/optcontext.h>
#include <wrap/cout.h>

using namespace utils::parser;

template <class T>
auto define_parser(const utils::Sequencer<T>& seq, expr::PlaceHolder*& ph) {
    using namespace utils::wrap;
    auto rp = expr::define_replacement(ph);
    auto mul = expr::define_binary(
        rp,
        expr::Ops{"*", expr::Op::mul},
        expr::Ops{"/", expr::Op::div},
        expr::Ops{"%", expr::Op::mod});
    auto add = expr::define_binary(
        mul,
        expr::Ops{"+", expr::Op::add},
        expr::Ops{"-", expr::Op::sub});
    auto and_ = expr::define_binary(
        add,
        expr::Ops{"&&", expr::Op::and_});
    auto exp = expr::define_binary(
        and_,
        expr::Ops{"||", expr::Op::or_});
    auto call = expr::define_callexpr<string, vector>(exp);
    auto prim = expr::define_primitive<string>(call);
    ph = expr::make_replacement(seq, expr::define_bracket(prim, exp));
    auto st = expr::define_command_struct<string, vector>(exp, true);
    auto parser = expr::define_set<string, vector>(st, false, false, "program");
    return parser;
}

auto& cout = utils::wrap::cout_wrap();

int main(int argc, char** argv) {
    using namespace utils::cmdline;
    option::Context ctx;
    auto input = ctx.String<utils::wrap::string>("input,i", "", "input file", "FILE", option::CustomFlag::required | option::CustomFlag::appear_once);
    auto output = ctx.String<utils::wrap::string>("output,o", "", "output file", "FILE", option::CustomFlag::required | option::CustomFlag::appear_once);
    auto err = option::parse_required(argc, argv, ctx, utils::helper::nop, option::ParseFlag::assignable_mode);
    if (auto msg = error_msg(err)) {
        cout << "error: " << ctx.erropt() << ": "
             << msg << "\n";
        return -1;
    }
}
