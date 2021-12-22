/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#pragma once
#include "../../include/wrap/lite/lite.h"
#include "../../include/syntax/make_parser/keyword.h"

namespace ifacegen {
    namespace utw = utils::wrap;

    struct Arg {
        utw::string name;
        size_t pointer = 0;
        utw::string type;
    };

    struct Interface {
        utw::string rettype;
        size_t pointer = 0;
        utw::string funcname;
        utw::vector<Arg> args;
        bool is_const = false;
    };

    struct FileData {
        utw::string pkgname;
        utw::map<utw::string, utw::vector<Interface>> ifaces;
    };

    struct State {
        utils::syntax::KeyWord prevv = utils::syntax::KeyWord::bos;
        utw::string prevtoken;
    };
}  // namespace ifacegen
