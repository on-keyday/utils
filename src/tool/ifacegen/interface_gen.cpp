/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// interface_gen - generate golang like interface

#include "../../include/cmdline/make_opt.h"
#include "../../include/cmdline/parse.h"

#include "../../include/syntax/syntaxc/make_syntaxc.h"

#include "../../include/wrap/cout.h"

#include "../../include/file/file_view.h"

int main(int argc, char** argv) {
    using namespace utils;
    using namespace utils::cmdline;
    auto& cout = wrap::cout_wrap();
    auto& cerr = wrap::cerr_wrap();
    DefaultDesc desc;
    desc.set("output-file,o", str_option(""), "set output file", OptFlag::need_value, "filename")
        .set("input-file,i", str_option(""), "set input file", OptFlag::need_value, "filename")
        .set("varbose,v", bool_option(true), "verbose log", OptFlag::none)
        .set("help,h", bool_option(true), "show help", OptFlag::none);
    int index = 1;
    DefaultSet result;
    utils::wrap::vector<wrap::string> arg;
    auto err = parse(index, argc, argv, desc, result, ParseFlag::optget_mode, &arg);
    if (err != ParseError::none) {
        cerr << "ifacegen: error: " << argv[index] << ": " << error_message(err) << "\n"
             << "try `ifacegen -h` for more info\n";
        return -1;
    }
    if (result.is_set("help")) {
        cout << desc.help(argv[0]);
        return 1;
    }
    auto in = result.is_set("input-file");
    auto out = result.is_set("output-file");
    if (!in || !out) {
        cerr << "ifacegen: error: need --input-file and --output-file option\n";
        return -1;
    }
    auto& infile = *in->value<wrap::string>();
    auto& outfile = *out->value<wrap::string>();
    cout << "process end\n";
    auto stxc = syntax::make_syntaxc();
    constexpr auto def = R"def(
        ROOT:=PACKAGE? INTERFACE*?
        PACKAGE:="package" ID!
        INTERFACE:="interface"[ ID "{" FUNCDEF*? "}" ]!
        FUNCDEF:=ID POINTER? [ID "(" FUNCLIST? ")" "const"?]!
        POINTER:="*"*
        FUNCLIST:=VARDEF ["," FUNCLIST! ]?
        VARDEF:=ID POINTER? "const"? ID
    )def";
    tokenize::Tokenizer<wrap::string> token;
    stxc->cb = [&](auto& ctx) {
        cout << ctx.stack.back() << ":" << ctx.result.token << "\n";
    };
    {
        auto seq = make_ref_seq(def);
        if (!stxc->make_tokenizer(seq, token)) {
            cerr << "ifacegen: " << stxc->error();
            return -1;
        }
    }
    decltype(token)::token_t tok;
    {
        file::View view;
        if (!view.open(infile)) {
            cerr << "ifacegen: error: file `" << infile << "` couldn't open\n";
            return -1;
        }
        auto sv = make_ref_seq(view);

        auto res = token.tokenize(sv, tok);
        assert(res && "ifacegen: fatal error: can't tokenize");
        const char* errmsg = nullptr;
        if (!tokenize::merge(errmsg, tok, tokenize::escaped_comment<wrap::string>("\"", "\\"))) {
            cerr << "ifacegen: error: " << errmsg;
            return -1;
        }
    }
    {
        auto r = syntax::Reader<wrap::string>(tok);
        if (!stxc->matching(r)) {
            cerr << "ifacegen: " << stxc->error();
        }
    }
}
