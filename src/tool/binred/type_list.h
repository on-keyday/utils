/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// binred - binary reader helper
#pragma once

#include "../../include/wrap/lite/lite.h"
#include "../../include/syntax/make_parser/keyword.h"
#include "../../include/syntax/matching/matching.h"

namespace binred {
    namespace utw = utils::wrap;
    enum class FlagType {
        none,
        eq,
        bit,
        ls,
        gt,
        nq,
        egt,
        els,
    };

    struct Val {
        utw::string val;
        utils::syntax::KeyWord kind = {};
    };

    enum class Op {
        none,
        add,
        sub,
    };

    struct Size {
        Val size1;
        Op op = Op::none;
        Val size2;
    };

    struct Flag {
        utw::string depend;
        Val val;
        FlagType type = FlagType::none;
        Size size;
    };

    struct Type {
        utw::string name;
        Flag flag;
    };

    struct Member {
        utw::string name;
        Type type;
        utw::string defval;
        utils::syntax::KeyWord kind = {};
    };

    struct Base {
        Type type;
    };

    struct Struct {
        Base base;
        utw::vector<Member>
            member;
    };

    struct FileData {
        utw::string pkgname;
        utw::map<utw::string, Struct> structs;
        utw::vector<utw::string> defvec;
        utw::string write_method;
        utw::string read_method;
        utw::vector<utw::string> imports;
    };

    struct State {
        FileData data;
        utw::string cuurent_struct;
    };

    enum class GenFlag {

    };

    bool read_fmt(utils::syntax::MatchContext<utw::string, utw::vector>& result, State& state);
    void generate_cpp(utw::string& str, FileData& data, GenFlag flag);

}  // namespace binred
