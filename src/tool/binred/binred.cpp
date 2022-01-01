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

int main(int argc, char** argv) {
    namespace us = utils::syntax;
    namespace uc = utils::cmdline;
    namespace utw = utils::wrap;
    int idx = 1;
    auto& cout = utils::wrap::cout_wrap();
    uc::DefaultDesc desc;
    uc::DefaultSet result;
    utw::vector<utw::string> arg;
    auto err = uc::parse(idx, argc, argv, desc, result, uc::ParseFlag::optget_mode, &arg);
    if (err != uc::ParseError::none) {
        cout << "binred: error: " << uc::error_message(err) << "\n";
        return -1;
    }
    constexpr auto def = R"(
        ROOT:=STRUCT*
        STRUCT:="struct" ID "{" MEMBER*? "}" EOS
        MEMBER:=ID TYPE!
        TYPE:=ID FLAG? "=" [INTEGER|STRING]
        FLAG:="?" ID ["eq"|"bit"] [INTEGER|STRING|ID]  
    )";
    auto c = us::make_syntaxc();
    auto s = us::make_tokenizer();
    auto seq = utils::make_ref_seq(def);

    if (!c->make_tokenizer(seq, s)) {
        cout << "binred: " << c->error();
        return -1;
    }

    c->cb = [&](auto& result) {
        cout << "";
    };
}