/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// binred - binary reader helper
#pragma once

#include "../../include/wrap/light/lite.h"
#include <deprecated/syntax/make_parser/keyword.h>
#include <deprecated/syntax/matching/matching.h>
#include <deprecated/syntax/tree/parse_tree.h>

namespace binred {
    namespace utw = futils::wrap;
    namespace us = futils::syntax;
    enum class FlagType {
        none,
        eq,
        bit,
        ls,
        gt,
        nq,
        egt,
        els,
        nbit,
        mod,
    };

    struct Val {
        utw::string val;
        futils::syntax::KeyWord kind = {};
    };

    enum class Op {
        none,
        add,
        sub,
        mod,
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

    struct Tree {
        us::KeyWord kw;
        utw::string token;
        utw::shared_ptr<Tree> left;
        utw::shared_ptr<Tree> right;
    };

    using tree_t = utw::shared_ptr<Tree>;

    struct Cond {
        tree_t tree;
        utw::string errvalue;
    };

    struct Type {
        utw::string name;
        utw::vector<Cond> prevcond;
        utw::vector<Cond> existcond;
        // utw::vector<tree_t> aftercond;
        tree_t size;
    };

    struct Member {
        utw::string name;
        Type type;
        utw::string defval;
        futils::syntax::KeyWord kind = {};
        utw::string errvalue;
    };

    struct Struct;

    struct Base {
        Type type;
        Struct* ptr = nullptr;
    };

    struct Struct {
        Base base;
        utw::string errtype;
        utw::vector<Member> member;
    };

    struct FileData {
        utw::string pkgname;
        utw::map<utw::string, Struct> structs;
        utw::vector<utw::string> defvec;
        utw::string write_method;
        utw::string read_method;
        utw::string make_ptr;
        utw::string ptr_type;
        utw::string defnone = "none";
        utw::string deferror = "error";
        utw::vector<utw::string> imports;
    };

    struct Manager {
        using tree_t = utw::shared_ptr<Tree>;
        tree_t make_tree(us::KeyWord kw, const utw::string& v) {
            auto ret = utw::make_shared<Tree>();
            ret->token = v;
            ret->kw = kw;
            return ret;
        }
        bool ignore(const utw::string& t) {
            return t == "(" || t == ")";
        }
    };

    struct State {
        FileData data;
        utw::string cuurent_struct;
        futils::syntax::tree::TreeMatcher<utw::string, utw::vector, Manager> tree;
    };

    enum class GenFlag {
        sep_namespace = 0x1,
        add_license = 0x2,
        dep_enc_is_raw_ptr = 0x4,
    };

    DEFINE_ENUM_FLAGOP(GenFlag)

    bool read_fmt(futils::syntax::MatchContext<utw::string, utw::vector>& result, State& state);
    void generate_cpp(utw::string& str, FileData& data, GenFlag flag);

}  // namespace binred
