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

    enum class Language {
        cpp,
    };

    enum class RefKind {
        none,
        rval,
        lval,
    };

    struct Type {
        RefKind ref = RefKind::none;
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
        utw::string default_result;
    };

    struct FileData {
        utw::string pkgname;
        utw::map<utw::string, utw::vector<Interface>> ifaces;
        utw::vector<utw::string> headernames;
        utw::map<utw::string, utw::string> aliases;
        bool has_ref_ret = false;
    };

    struct State {
        FileData data;
        utw::string current_iface;
        Interface iface;
        std::string current_alias;
    };

    bool read_callback(utils::syntax::MatchContext<utw::string, utw::vector>& result, State& state);
    bool generate(FileData& data, utw::string& str, Language lang, bool expand_alias);
}  // namespace ifacegen
