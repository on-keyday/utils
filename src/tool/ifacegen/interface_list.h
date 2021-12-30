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
        bool vararg = false;
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

    struct TypeName {
        bool vararg = false;
        utw::string name;
        utw::string defvalue;
    };

    struct IfaceList {
        utw::vector<TypeName> typeparam;
        utw::vector<Interface> iface;
    };

    struct Alias {
        utw::string token;
        bool is_macro = false;
        utw::vector<TypeName> types;
    };

    struct FileData {
        utw::string pkgname;
        utw::map<utw::string, IfaceList> ifaces;
        utw::vector<utw::string> defvec;
        utw::vector<utw::string> headernames;
        utw::map<utw::string, Alias> aliases;
        bool has_ref_ret = false;
        utw::string typeid_func;
        utw::string typeid_type;
        utw::string helper_deref;
    };

    struct State {
        FileData data;
        utw::string current_iface;
        bool vararg = false;
        utw::vector<TypeName> types;
        Interface iface;
        std::string current_alias;
    };

    enum class GenFlag {
        none = 0,
        expand_alias = 0x1,
        no_vtable = 0x2,  // for windows
        add_license = 0x4,
        not_accept_null = 0x8,
        use_dyn_cast = 0x10,
        use_allocator = 0x20,
    };

    DEFINE_ENUM_FLAGOP(GenFlag)

    bool read_callback(utils::syntax::MatchContext<utw::string, utw::vector>& result, State& state);
    bool generate(FileData& data, utw::string& str, Language lang, GenFlag flag);
}  // namespace ifacegen
