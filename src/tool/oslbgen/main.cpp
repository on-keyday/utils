/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include "gen.h"
#include "type.h"
#include <comb2/tree/branch_table.h>
#include <file/file_view.h>
#include <code/src_location.h>
#include <fstream>
struct Flags : futils::cmdline::templ::HelpOption {
    std::string input;
    std::string output;

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString(&input, "i,input", "input file", "FILE", futils::cmdline::option::CustomFlag::required);
        ctx.VarString(&output, "o,output", "output file", "FILE");
    }
};
auto& cout = futils::wrap::cout_wrap();
auto& cerr = futils::wrap::cerr_wrap();

struct Ctx : futils::comb2::tree::BranchTable {
    void error(auto&&... err) {
        cerr << "oslbgen: error: ";
        (cerr << ... << err);
        cerr << "\n";
    }
};

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    Ctx c;
    futils::file::View view;
    if (!view.open(flags.input)) {
        cerr << "oslbgen: error: failed to open file: " << flags.input << "\n";
        return 1;
    }
    auto seq = futils::make_ref_seq(view);
    auto dump_err = [&](oslbgen::Pos pos) {
        std::string buf;
        seq.rptr = pos.begin;
        auto src_loc = futils::code::write_src_loc(buf, seq);
        cerr << "osslbgen: error: parse failed at here:\n"
             << flags.input << ":" << src_loc.line << ":" << src_loc.pos << ":\n"
             << buf << "\n";
        return 2;
    };
    if (auto m = oslbgen::parse(seq, c); m != futils::comb2::Status::match) {
        return dump_err(oslbgen::Pos{.begin = seq.rptr});
    }
    auto col = oslbgen::node::collect(c.root_branch);
    oslbgen::DataSet db;
    db.ns = "ssl_import";
    if (auto pos = oslbgen::node_root(db, col); !pos.npos()) {
        return dump_err(pos);
    }
    oslbgen::Out o;
    if (auto pos = oslbgen::write_code(o, db); !pos.npos()) {
        return dump_err(pos);
    }
    if (flags.output.size() == 0) {
        cout << o.out();
        return 0;
    }
    std::ofstream fs{flags.output, std::ios::binary | std::ios::trunc};
    if (!fs) {
        return 1;
    }
    fs << o.out();
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
