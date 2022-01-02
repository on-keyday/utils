/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "type_list.h"

#include "../../include/syntax/syntaxc/make_syntaxc.h"

#include "../../include/wrap/cout.h"

#include "../../include/cmdline/parse.h"
#include "../../include/cmdline/make_opt.h"

#include "../../include/file/file_view.h"

#include <fstream>

int main(int argc, char** argv) {
    namespace us = utils::syntax;
    namespace uc = utils::cmdline;
    namespace utw = utils::wrap;
    int idx = 1;
    auto& cout = utils::wrap::cout_wrap();
    auto& cerr = utils::wrap::cerr_wrap();
    auto devbug = [&] {
        cerr << "this is library bug\n"
             << "please report bug to "
             << "https://github.com/on-keyday/utils \n";
    };
    uc::DefaultDesc desc;
    desc
        .set("help,h", uc::bool_option(true), "show option help", uc::OptFlag::once_in_cmd)
        .set("input,i", uc::str_option(""), "input file", uc::OptFlag::once_in_cmd, "filename")
        .set("output,o", uc::str_option(""), "output file", uc::OptFlag::once_in_cmd, "filename")
        .set("verbose,v", uc::bool_option(true), "verbose log", uc::OptFlag::once_in_cmd)
        .set("write-method,w", uc::str_option("write"), "set write method", uc::OptFlag::once_in_cmd, "funcname")
        .set("read-method,r", uc::str_option("read"), "set read method", uc::OptFlag::once_in_cmd, "funcname");
    uc::DefaultSet result;
    utw::vector<utw::string> arg;
    auto err = uc::parse(idx, argc, argv, desc, result, uc::ParseFlag::optget_mode, &arg);
    if (err != uc::ParseError::none) {
        cout << "binred: error: " << uc::error_message(err) << "\n";
        return -1;
    }
    if (result.is_true("help")) {
        cout << desc.help(argv[0]);
        return 1;
    }
    constexpr auto def = R"(
        ROOT:=PACKAGE? [STRUCT|IMPORT]*?
        PACKAGE:="package" ID!
        IMPORT:="import" UNTILEOL
        STRUCT:="struct" ID "{" MEMBER*? "}" EOS
        MEMBER:=ID TYPE!
        TYPE:=ID SIZE? FLAG? ["=" [INTEGER|STRING]]?
        FLAG:="?" ID ["eq"|"bit"|"ls"|"gt"] [INTEGER|STRING|ID]
        SIZE:="$" [INTEGER|ID]  
    )";
    auto c = us::make_syntaxc();
    auto s = us::make_tokenizer();
    auto seq = utils::make_ref_seq(def);

    if (!c->make_tokenizer(seq, s)) {
        cout << "binred: " << c->error();
        return -1;
    }

    s.symbol.predef.push_back("#");
    decltype(s)::token_t tok;
    auto infile = result.has_value<utw::string>("input");
    auto outfile = result.has_value<utw::string>("output");
    if (!infile || !outfile) {
        cerr << "binred: error: need --input and --output\ntry binred -h for help\n";
        return -1;
    }
    {
        utils::file::View view;
        if (!view.open(*infile)) {
            cerr << "binred: error: file `" << *infile << "` couldn't open\n";
            return -1;
        }
        auto sv = utils::make_ref_seq(view);
        if (!s.tokenize(sv, tok)) {
            cerr << "binred: fatal error: failed to tokenize\n";
            devbug();
            return -1;
        }
    }

    {
        const char* errmsg = nullptr;
        if (!utils::tokenize::merge(errmsg, tok,
                                    utils::tokenize::line_comment<utw::string>("#"),
                                    utils::tokenize::escaped_comment<utw::string>("\"", "\\"))) {
            cerr << "binred: error: " << errmsg << "\n";
            return -1;
        }
    }
    binred::State state;
    state.data.write_method = "write";
    state.data.read_method = "read";
    if (auto wm = result.has_value<utw::string>("write-method")) {
        state.data.write_method = *wm;
    }
    if (auto rm = result.has_value<utw::string>("read-method")) {
        state.data.read_method = *rm;
    }
    c->cb = [&](auto& ctx) {
        if (result.is_true("verbose")) {
            cout << ctx.top() << ":" << us::keywordv(ctx.kind()) << ":" << ctx.token() << "\n";
        }
        return binred::read_fmt(ctx, state) ? us::MatchState::succeed : us::MatchState::fatal;
    };
    {
        auto r = us::Reader<utw::string>(tok);
        if (!c->matching(r)) {
            cerr << "binred: " << c->error();
            return -1;
        }
    }
    utw::string str;
    binred::generate_cpp(str, state.data, {});
    if (result.is_true("verbose")) {
        cout << "generated code:\n";
        cout << str;
    }
    {
        std::ofstream fs(*outfile);
        if (!fs.is_open()) {
            cerr << "ifacegen: error:file `" << *outfile << "` couldn't open\n";
            return -1;
        }
        fs << str;
    }
    cout << "generated\n";
}
