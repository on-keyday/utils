/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cmdline/template/parse_and_err.h>

#include <wrap/argv.h>

#include "vstack.h"
#include "func.h"
#include "writer.h"
#include "ssa.h"

using namespace utils;

struct Option : cmdline::templ::HelpOption {
    void bind(cmdline::option::Context& c) {
        bind_help(c);
    }
};

int main(int argc, char** argv) {
    wrap::U8Arg _(argc, argv);
    Option opt;
    auto out = vstack::make_out();
    auto main_ = [&](Option& opt, cmdline::option::Context& c) {
        auto seq = make_ref_seq(R"(
    fn add(a,b,c) {
        return a+b+c
    } 
    
    fn main() {
        let a,b = 1,2
        return add(a,b,3)
    }
)");
        auto src = vstack::token::make_source<vstack::Log>(
            std::move(seq),
            vstack::symbol::make_context(
                vstack::tokens(),
                vstack::token::defined_primitive(),
                vstack::statement(), vstack::expr()));
        auto dst = vstack::parse(src);
        if (!vstack::symbol::resolve_vars(src.log, "vstack_", 0, src.ctx.refs)) {
            src.log.token_with_seq(src.seq, src.log.tok);
            return 1;
        }
        vstack::ssa::SSA<> ops;
        vstack::middle::global_to_ssa(ops, dst);
        /*vstack::func::walk(dst);
        vstack::write::Stacks stack;
        stack.ctx.vstack_type = "VStack";
        stack.refs = &src.ctx.refs;
        vstack::gen::write_global(stack, dst);
        out(stack.glyobal.header.string(), "\n", stack.global.src.string());
        */
        vstack::write::Stacks stack;
        stack.ctx.vstack_type = "VStack";
        stack.refs = &src.ctx.refs;
        vstack::gen::write_ssa(stack, stack.global.src, ops);
        out(stack.global.header.string());
        return 0;
    };
    return cmdline::templ::parse_or_err<std::string>(argc, argv, opt, out, main_);
}
