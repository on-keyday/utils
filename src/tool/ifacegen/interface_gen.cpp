/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// interface_gen - generate golang like interface

#include "../../include/cmdline/make_opt.h"
#include "../../include/cmdline/parse.h"

#include "../../include/syntax/syntaxc/make_syntaxc.h"

#include "../../include/wrap/cout.h"

#include "../../include/file/file_view.h"

#include "interface_list.h"

#include <fstream>

int main(int argc, char** argv) {
    using namespace utils;
    using namespace utils::cmdline;
    auto& cout = wrap::cout_wrap();
    auto& cerr = wrap::cerr_wrap();
    auto dev_bug = [&] {
        cerr << "this is a library bug\n";
        cerr << "please report bug to ";
        cerr << "https://github.com/on-keyday/utils\n";
    };
    DefaultDesc desc;
    desc.set("output-file,o", str_option(""), "set output file", OptFlag::need_value, "filename")
        .set("input-file,i", str_option(""), "set input file", OptFlag::need_value, "filename")
        .set("verbose,v", bool_option(true), "verbose log", OptFlag::once_in_cmd)
        .set("help,h", bool_option(true), "show option help", OptFlag::none)
        .set("syntax,s", bool_option(true), "show syntax help", OptFlag::none)
        .set("header,H", str_option(""), "additional header file", OptFlag::need_value, "filename")
        .set("expand,e", bool_option(true), "expand macro (alias is not expanded)", OptFlag::once_in_cmd)
        .set("no-vtable,V", bool_option(true), "add __declspec(novtable) (for windows)", OptFlag::once_in_cmd)
        .set("no-rtti", multi_option<wrap::string>(2), "use type and func instead of `const std::type_info&` and `typeid(T__)`", OptFlag::once_in_cmd, "type func")
        .set("license", bool_option(true), "add /*license*/", OptFlag::once_in_cmd)
        .set("not-accept-null,n", bool_option(true), "not accept nullptr-object", OptFlag::once_in_cmd)
        .set("helper-deref", str_option("<helper/deref>"), "helper deref location", OptFlag::once_in_cmd)
        .set("use-dynamic-cast,d", bool_option(true), "use dynamic cast for type assert", OptFlag::once_in_cmd)
        .set("separate,S", bool_option(true), "separate namespace by ::", OptFlag::once_in_cmd)
        .set("independent,D", bool_option(true), "insert deref code not to depend utils", OptFlag::once_in_cmd)
        .set("need-typeid,T", bool_option(true), "add type id func with type assert func", OptFlag::once_in_cmd);
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
    if (result.is_set("syntax")) {
        cout << R"(Syntax:
    #COMMENT
    
    interface NAME{
        const FUNC(ARG &*const TYPE,ARG2 TYPE) TYPE = DEFAULT_RETURN_VALUE
    }

    typeparam T, ...Args
    interface NAME{}

    import <HEADERNAME>

    import "HEADERNAME"

    macro NAME REFERRED

    alias NAME REFERRED

    typeparam T,^U 
    # T will be `typename T`, U will be `template<typename...>typename U` 
    alias NAME REFERRED

    

Special Value:
    Func Name:
        decltype - generate type assertion func ex) decltype() type (`type` is placeholder)
        __copy__ - generate copy constructor and copy assign ex) __copy__() type (`type` is placeholder)
        __call__ - generate `operator()` ex) __call__(num int) int 
        __array__ - generate `operator[]` ex)__array__(index size_t) char
        __unsafe__ - generate unsafe raw pointer getter (short cut)
    Default Value:
        panic - generate throw std::bad_function_call()
)";
        return 1;
    }
    auto in = result.is_set("input-file");
    auto out = result.is_set("output-file");
    if (!in || !out) {
        cerr << "ifacegen: error: need --input-file and --output-file option\n"
             << "try `ifacegen -h` for more info\n";

        return -1;
    }
    auto& infile = *in->value<wrap::string>();
    auto& outfile = *out->value<wrap::string>();
    bool verbose = false;
    ifacegen::GenFlag flag = {};
    if (auto v = result.is_set("verbose"); v && *v->value<bool>()) {
        verbose = true;
    }
    if (auto v = result.is_set("expand"); v && *v->value<bool>()) {
        flag |= ifacegen::GenFlag::expand_alias;
    }
    if (auto v = result.is_set("no-vtable"); v && *v->value<bool>()) {
        flag |= ifacegen::GenFlag::no_vtable;
    }
    if (auto v = result.is_set("license"); v && *v->value<bool>()) {
        flag |= ifacegen::GenFlag::add_license;
    }
    if (auto v = result.is_set("not-accept-null"); v && *v->value<bool>()) {
        flag |= ifacegen::GenFlag::not_accept_null;
    }
    if (auto v = result.is_set("use-dynamic-cast"); v && *v->value<bool>()) {
        flag |= ifacegen::GenFlag::use_dyn_cast;
    }
    if (auto v = result.is_set("separate"); v && *v->value<bool>()) {
        flag |= ifacegen::GenFlag::sep_namespace;
    }
    if (auto v = result.is_set("independent"); v && *v->value<bool>()) {
        flag |= ifacegen::GenFlag::not_depend_lib;
    }
    if (result.is_true("need-typeid")) {
        flag |= ifacegen::GenFlag::need_typeidfun;
    }
    ifacegen::State state;
    if (auto h = result.is_set("header")) {
        if (auto s = h->value<wrap::string>()) {
            state.data.headernames.push_back(*s);
        }
        else if (auto vs = h->value<wrap::vector<OptValue<>>>()) {
            for (auto& s : *vs) {
                if (auto v = s.value<wrap::string>()) {
                    state.data.headernames.push_back(*v);
                }
            }
        }
    }
    if (auto h = result.is_set("helper-deref")) {
        if (auto s = h->value<wrap::string>()) {
            state.data.helper_deref = *s;
        }
    }
    if (auto h = result.is_set("no-rtti")) {
        if (auto s = h->value<wrap::vector<wrap::string>>()) {
            state.data.typeid_type = s->at(0);
            state.data.typeid_func = s->at(1);
        }
    }
    auto stxc = syntax::make_syntaxc();
    constexpr auto def = R"def(
        ROOT:=PACKAGE? [TYPEPARAM? [INTERFACE|ALIAS]|MACRO|IMPORT]*? EOF
        PACKAGE:="package" ID!
        IMPORT:="import" UNTILEOL
        MACRO:= "macro" ID UNTILEOL
        ALIAS:="alias" ID UNTILEOL
        INTERFACE:= "interface"[ ID "{" FUNCDEF*? "}" ]! EOS
        TYPEPARAM:="typeparam" TYPENAME ["," TYPENAME]*?
        TYPENAME:="..." ID!|TYPEID DEFTYPE?
        TYPEID:="^"? ID 
        DEFTYPE:="=" ID
        FUNCDEF:="const"? "noexcept"? ID ["(" FUNCLIST? ")" TYPE ["=" ID!]? ]! EOS
        POINTER:="*"*
        FUNCLIST:=VARDEF ["," FUNCLIST! ]?
        VARDEF:=ID TYPE
        TYPE:="..."? ["&&"|"&"]? POINTER? "const"? ID
    )def";

    tokenize::Tokenizer<wrap::string, wrap::vector> token;

    stxc->cb = [&](auto& ctx) {
        if (verbose) {
            cout << ctx.stack.back() << ":" << syntax::keywordv(ctx.result.kind) << ":" << ctx.result.token << "\n";
        }
        if (!ifacegen::read_callback(ctx, state)) {
            return syntax::MatchState::fatal;
        }
        return syntax::MatchState::succeed;
    };
    {
        auto seq = make_ref_seq(def);
        if (!stxc->make_tokenizer(seq, token)) {
            cerr << "ifacegen: " << stxc->error();
            dev_bug();
            return -1;
        }
    }
    token.symbol.predef.push_back("#");
    decltype(token)::token_t tok;
    {
        file::View view;
        if (!view.open(infile)) {
            cerr << "ifacegen: error: file `" << infile << "` couldn't open\n";
            return -1;
        }
        auto sv = make_ref_seq(view);

        auto res = token.tokenize(sv, tok);
        if (!res) {
            cerr << "ifacegen: fatal error: can't tokenize\n";
            dev_bug();
            return -1;
        }
        const char* errmsg = nullptr;
        if (!tokenize::merge(errmsg, tok,
                             tokenize::line_comment<wrap::string>("#"),
                             tokenize::escaped_comment<wrap::string>("\"", "\\"))) {
            cerr << "ifacegen: error: " << errmsg;
            return -1;
        }
    }
    {
        auto r = syntax::Reader<wrap::string>(tok);
        if (!stxc->matching(r)) {
            cerr << "ifacegen: " << stxc->error();
            return -1;
        }
    }
    wrap::string got;
    ifacegen::generate(state.data, got, ifacegen::Language::cpp, flag);
    if (verbose) {
        cout << "generated code:\n";
        cout << got;
    }
    {
        std::ofstream fs(outfile);
        if (!fs.is_open()) {
            cerr << "ifacegen: error:file `" << outfile << "` couldn't open\n";
            return -1;
        }
        fs << got;
    }
    cout << "generated\n";
}
