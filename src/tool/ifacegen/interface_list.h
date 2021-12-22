/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#pragma once
#include "../../include/wrap/lite/lite.h"
#include "../../include/syntax/make_parser/keyword.h"
#include "../../include/syntax/matching/matching.h"

namespace ifacegen {
    namespace utw = utils::wrap;

    struct Type {
        bool is_const = false;
        size_t pointer = 0;
        utw::string prim;
    };

    struct Arg {
        utw::string name;
        Type type;
    };

    struct Interface {
        Type type;
        utw::string funcname;
        utw::vector<Arg> args;
        bool is_const = false;
    };

    struct FileData {
        utw::string pkgname;
        utw::map<utw::string, utw::vector<Interface>> ifaces;
    };

    struct State {
        FileData data;
        utw::string current_iface;
        Interface iface;
    };

    bool read_callback(utils::syntax::MatchContext<utw::string, utw::vector>& result, State& state);

}  // namespace ifacegen
