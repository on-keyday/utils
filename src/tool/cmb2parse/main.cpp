/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <comb2/composite/string.h>
#include <comb2/composite/range.h>
#include <comb2/basic/group.h>
#include <comb2/dynamic/dynamic.h>
#include <memory>
#include <string>
#include <string_view>
#include "code/src_location.h"
#include "comb2/status.h"
#include "comb2/tree/simple_node.h"
#include "core/sequencer.h"
#include "file/file_view.h"
#include <wrap/cin.h>
#include "grammar.h"
struct Flags : futils::cmdline::templ::HelpOption {
    std::string_view definition;
    std::string_view input = "-";
    bool json = false;
    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString<true>(&definition, "d,definition", "Syntax definition file", "FILE", futils::cmdline::option::CustomFlag::required);
        ctx.VarString<true>(&input, "i,input", "input file", "FILE");
        ctx.VarBool(&json, "j,json", "output parse tree as json");
    }
};

int top_down_loop(Flags& flags, Description& desc, auto&& do_action) {
    TopdownTable rtable;
    auto r = convert_topdown(std::move(desc));
    if (!r) {
        return -1;
    }
    rtable = std::move(*r);
    auto handle_ident = [&](std::string_view input) {
        return do_topdown_parse(rtable, input, flags.input, flags.json);
    };
    return do_action(handle_ident);
}

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    Description desc;
    futils::file::View view;
    if (!view.open(flags.definition)) {
        cerr << "cmb2parse: error: failed to open file " << flags.definition << "\n";
        return -1;
    }
    auto seq = futils::make_ref_seq(view);
    Table table;
    if (grammar::parse(seq, table) != futils::comb2::Status::match) {
        std::string out;
        auto src_loc = futils::code::write_src_loc(out, seq);
        cerr << "cmb2parse: error: failed to parse\n";
        cerr << table.buffer << "\n";
        cerr << flags.definition << ":" << src_loc.line + 1 << ":" << src_loc.pos + 1 << "\n";
        cerr << out << "\n";
        return -1;
    }
    auto root = futils::comb2::tree::node::collect<grammar::NodeKind>(table.root_branch);
    if (!root) {
        cerr << "cmb2parse: error: failed to collect parse tree\n";
        return -1;
    }
    if (!analyze_description(desc, root)) {
        return -1;
    }
    auto main_loop = [&](auto&& do_parse) {
        if (flags.input != "-") {
            futils::file::View view;
            if (!view.open(flags.input)) {
                cerr << "cmb2parse: error: failed to open file " << flags.input << "\n";
                return -1;
            }
            return do_parse(std::string_view((const char*)view.data(), view.size()));
        }
        else {
            auto& cin = futils::wrap::cin_wrap();
            std::string line;
            while (true) {
                cout << "> ";
                line.clear();
                cin >> line;
                while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                    line.pop_back();
                }
                if (line.empty()) {
                    break;
                }
                do_parse(line);
            }
        }
        return 0;
    };
    return top_down_loop(flags, desc, main_loop);
}

int main(int argc, char** argv) {
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
