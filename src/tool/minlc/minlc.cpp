/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "middle/parser.h"
#include "middle/middle.h"
#include <space/line_pos.h>
#include <file/file_view.h>
#include <cmdline/option/optcontext.h>
#include <wrap/argv.h>
#include "backend/llvm/llvm.h"
#include <filesystem>
utils::Sequencer<utils::file::View&>* srcptr;
namespace minlc {
    void print_node(const std::shared_ptr<utils::minilang::MinNode>& node) {
        std::string w;
        srcptr->rptr = node->pos.begin;
        const auto len = node->pos.end - node->pos.begin;
        utils::space::write_src_loc(w, *srcptr, len);
        printe(w);
    }

    void print_pos(size_t pos) {
        std::string w;
        srcptr->rptr = pos;
        utils::space::write_src_loc(w, *srcptr, 1);
        printe(w);
    }
}  // namespace minlc

namespace mc = minlc;
namespace opt = utils::cmdline::option;
namespace fs = std::filesystem;

struct Option {
    std::string input;
    bool help;
    void bind(opt::Context& ctx) {
        ctx.VarString(&input, "i,input", "input file", "FILE");
        ctx.VarBool(&help, "h,help", "show this help");
    }
} option;

int assemble() {
    utils::file::View view;
    if (!view.open(option.input)) {
        mc::printe("invalid path ", option.input);
        return -1;
    }

    auto seq = utils::make_ref_seq(view);
    srcptr = &seq;
    minlc::ErrC errc{};
    auto ok = mc::parse(seq, errc);
    if (!ok.first || !ok.second) {
        mc::printe("parse failed");
        return -1;
    }
    mc::middle::M m;
    auto prog = mc::middle::minl_to_program(m, ok.second);
    if (!prog) {
        mc::printe("mapping failed");
        return -1;
    }
    mc::llvm::L l{m};
    auto path = fs::path(option.input);
    auto genr = path.filename().generic_u8string();
    if (!mc::llvm::write_program(l, (const char*)genr.c_str(), prog)) {
        mc::printe("llvm output failed");
        return -1;
    }
    mc::printo(l.output);
    return 0;
}

int main(int argc, char** argv) {
    utils::wrap::U8Arg _(argc, argv);
    opt::Context ctx;
    option.bind(ctx);
    auto err = opt::parse_required(argc, argv, ctx, utils::helper::nop, opt::ParseFlag::assignable_mode);
    if (opt::perfect_parsed(err) && option.help) {
        mc::printo(ctx.Usage<std::string>(opt::ParseFlag::assignable_mode, argv[0]));
        return 1;
    }
    if (auto msg = opt::error_msg(err)) {
        mc::printe("error: ", ctx.erropt(), ": ", msg);
        return 1;
    }
    return assemble();
}
