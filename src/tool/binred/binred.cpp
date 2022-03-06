/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "type_list.h"

#include "../../include/syntax/syntaxc/make_syntaxc.h"

#include "../../include/wrap/cout.h"

#include "../../include/cmdline/parse.h"
#include "../../include/cmdline/make_opt.h"

#include "../../include/file/file_view.h"

#include "../../include/cmdline/option/optcontext.h"

#include <fstream>

#include <functional>

int main(int argc, char** argv) {
    auto f = std::bind([](int) {}, 2);
    f();
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
    uc::option::Context opt;
    uc::option::CustomFlag cu = uc::option::CustomFlag::appear_once;
    binred::State state;
    state.data.write_method = "write";
    state.data.read_method = "read";
    auto help = opt.Bool("help,h", false, "show option help", cu);
    auto infile = opt.String<utw::string>("input,i", "", "input file", "FILE", cu);
    auto outfile = opt.String<utw::string>("output,o", "", "output file", "FILE", cu);
    auto verbose = opt.Bool("verbose,v", false, "verbose help");
    auto smart_ptr = opt.VecString<utw::string>("smart-ptr,S", 2, "set ptr-like object template and function", "TEMPLATE FUNC", cu);
    opt.UnboundString<utw::string>("import,I", "set additional import header", "FILE");
    opt.VarString(&state.data.write_method, "write-method,w", "set write method", "METHOD", cu);
    opt.VarString(&state.data.read_method, "read-method,r", "set read method", "METHOD", cu);
    auto syntax = opt.Bool("syntax,s", false, "show syntax help", cu);
    auto genflag = opt.FlagSet("license", binred::GenFlag::add_license, "add /*license*/", cu);
    opt.VarFlagSet(genflag, "separate,p", binred::GenFlag::sep_namespace, "separate namespace", cu);
    opt.VarFlagSet(genflag, "encode_raw_ptr,R", binred::GenFlag::dep_enc_is_raw_ptr, "dependency encode is made with raw ptr", cu);
    auto none_error = opt.VecString<utw::string>("none-error,e", 2, "set default `none` and `error` enum name", "NONE ERROR", cu);
    /*
    uc::DefaultDesc desc;
    desc
        .set("help,h", uc::bool_option(true), "show option help", uc::OptFlag::once_in_cmd)
        .set("input,i", uc::str_option(""), "input file", uc::OptFlag::once_in_cmd, "filename")
        .set("output,o", uc::str_option(""), "output file", uc::OptFlag::once_in_cmd, "filename")
        .set("verbose,v", uc::bool_option(true), "verbose log", uc::OptFlag::once_in_cmd)
        .set("smart-ptr,S", uc::multi_option<utw::string>(2, 2), "set ptr-like object template and function", uc::OptFlag::once_in_cmd, "template funcname")
        .set("import,I", uc::str_option(""), "set additional import header", uc::OptFlag::none, "imports")
        .set("write-method,w", uc::str_option("write"), "set write method", uc::OptFlag::once_in_cmd, "funcname")
        .set("read-method,r", uc::str_option("read"), "set read method", uc::OptFlag::once_in_cmd, "funcname")
        .set("syntax,s", uc::bool_option(true), "syntax help", uc::OptFlag::once_in_cmd)
        .set("license", uc::bool_option(true), "add *license*
    ", uc::OptFlag::once_in_cmd)
        .set("separate,p", uc::bool_option(true), "separate namespace", uc::OptFlag::once_in_cmd)
        .set("none-error,e", uc::multi_option<utw::string>(2, 2), "set default `none` and `error` enum name", uc::OptFlag::once_in_cmd, "none error");
    uc::DefaultSet result;
    utw::vector<utw::string> arg;
    auto err = uc::parse(idx, argc, argv, desc, result, uc::ParseFlag::optget_mode, &arg);
    *
    if (err != uc::ParseError::none) {
        cout << "binred: error: " << uc::error_message(err) << "\n";
        return -1;
    }*/
    auto parse_mode = uc::option::ParseFlag::assignable_mode | uc::option::ParseFlag::assign_anyway_val;
    auto err = uc::option::parse(argc, argv, opt, utils::helper::nop, parse_mode);
    if (auto msg = error_msg(err)) {
        cout << "binred: error: " << opt.erropt() << ": " << msg << "\n";
        return -1;
    }
    if (*help) {
        // cout << desc.help(argv[0]);
        cout << opt.Usage<utw::string>(parse_mode, argv[0]);
        return 1;
    }
    if (*syntax) {
        cout << R"(Syntax:

    # COMMENT
    package NAMESPACE_NAME

    import <HEADER_NAME>
    import "HEADER_NAME"

    struct STRUCT_NAME - BASE_STRUCT {
        MEMBER TYPE ? MEMBER != 98 ! 0 ^ true||false
        MEMBER2 TYPE2 $ 30 = DEFAULT_VALUE
        MEMBER3 TYPE3 ? COND @ COND_ERROR_VALUE ! READ_ERROR_VALUE
    }
)";
        return 1;
    }
    constexpr auto def = R"a(
        ROOT:=PACKAGE? [STRUCT|IMPORT]*? EOF
        PACKAGE:="package" ID!
        IMPORT:="import" UNTILEOL
        STRUCT:="struct" ID BASE? ERRTYPE? "{" MEMBER*? "}" EOS
        BASE:="-" TYPE!
        ERRTYPE:="," ID!
        MEMBER:=ID TYPE! ["!" AS_RESULT]? ASSIGN?
        ASSIGN:="=" [INTEGER|ID]!
        TYPE:=ID SIZE? [PREV|EXIST]*?
        PREV:="?" FLAG_DETAIL! ["@" AS_RESULT]?
        EXIST:="^" FLAG_DETAIL!
        FLAG_DETAIL:=EXPR EOS
        AS_RESULT:=[INTEGER|ID]!
        SIZE:="$" EXPR EOS

        EXPR:=OR
        OR:=BOS AND ["||" AND!]*? EOS
        AND:=BOS EQ ["&&" EQ!]*? EOS
        EQ:=BOS ADD [["=="|"!="|">="|"<="|">"|"<"] ADD!]*? EOS
        ADD:=BOS MUL [["+"|"-"|"&"|"|"] MUL!]*? EOS
        MUL:=BOS PRIM [["*"|"/"|"%"] PRIM!]*? EOS
        PRIM:=BOS [INTEGER|IDs|"(" EXPR! ")"!]! EOS
        IDs:=BOS ID ["." ID!]*? EOS
    )a";
    auto c = us::make_syntaxc();
    auto s = us::make_tokenizer();
    auto seq = utils::make_ref_seq(def);

    if (!c->make_tokenizer(seq, s)) {
        cout << "binred: " << c->error();
        return -1;
    }

    s.symbol.predef.push_back("#");
    decltype(s)::token_t tok;
    /*auto infile = result.has_value<utw::string>("input");
    auto outfile = result.has_value<utw::string>("output");*/
    if (!infile->size() || !outfile->size()) {
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
    state.data.ptr_type = smart_ptr->at(0);
    state.data.make_ptr = smart_ptr->at(1);
    state.data.defnone = none_error->at(0);
    state.data.deferror = none_error->at(1);
    for (auto& arg : opt.find("import")) {
        state.data.imports.push_back(*arg.value.get_ptr<utw::string>());
    }
    c->cb = [&](auto& ctx) {
        if (*verbose) {
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
    // return 0;
    utw::string str;
    binred::generate_cpp(str, state.data, *genflag);
    if (*verbose) {
        cout << "generated code:\n";
        cout << str;
    }
    // return 0;
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
