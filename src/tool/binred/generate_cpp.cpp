/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "type_list.h"

#include "../../include/helper/appender.h"

namespace binred {
    namespace hlp = utils::helper;
    using tree_t = utw::shared_ptr<Tree>;

    void render_tree(utw::string& str, tree_t& tree, const utw::string& rel) {
        if (tree->left || tree->right) {
            hlp::append(str, "(");
        }
        if (tree->left) {
            render_tree(str, tree->left, rel);
        }
        if (tree->kw == us::KeyWord::id) {
            if (tree->token != "true" && tree->token != "false" &&
                tree->token != "nullptr" && rel.size() && str.back() != '.' &&
                !hlp::contains(tree->token, "::")) {
                hlp::appends(str, rel, ".");
            }
        }
        hlp::append(str, tree->token);
        if (tree->right) {
            render_tree(str, tree->right, rel);
        }
        if (tree->left || tree->right) {
            hlp::append(str, ")");
        }
    }

    void write_indent(utw::string& str, size_t indent) {
        for (size_t i = 0; i < indent; i++) {
            hlp::append(str, "    ");
        }
    }

    void generate_flag_cond_begin(utw::string& str, auto& in, tree_t& flag, bool not_ = false) {
        hlp::append(str, "if (");
        render_tree(str, flag);
        hlp::append(str, ") {\n");
        write_indent(str, 1);
    }

    void set_val(utw::string& str, Val& val, auto& in) {
        if (val.kind == utils::syntax::KeyWord::id) {
            hlp::appends(str, in, ".");
        }
        hlp::appends(str, val.val);
    }

    void render_cpp_errorcode(utw::string& str, Struct& st, Cond* cond, Member* memb, FileData& data) {
        if (cond && cond->errvalue.size()) {
            hlp::append(str, cond->errvalue);
        }
        else if (memb && memb->errvalue.size()) {
            hlp::append(str, memb->errvalue);
        }
        else if (st.errtype == "bool") {
            hlp::append(str, "false");
        }
        else if (st.errtype == "int") {
            hlp::append(str, "-1");
        }
        else {
            hlp::appends(str, st.errtype, "::", data.deferror);
        }
    }

    void render_cpp_cond_err(utw::string& str, utw::string& errtype, FileData& data) {
        if (errtype == "bool") {
            hlp::append(str, "!e__");
        }
        else if (errtype == "int") {
            hlp::append(str, "e__!=0");
        }
        else {
            hlp::appends(str, errtype, "::", data.defnone, "!=e__");
        }
    }

    void render_cpp_succeed_res(utw::string& str, utw::string& errtype, FileData& data) {
        if (errtype == "bool") {
            hlp::append(str, "true");
        }
        else if (errtype == "int") {
            hlp::append(str, "0");
        }
        else {
            hlp::appends(str, errtype, "::", data.defnone);
        }
    }

    void generate_with_cond(FileData& data, utw::string& str, Struct& st, Member& memb, auto& target,
                            auto& method, auto& out, auto& coder, bool check_after, bool check_failed) {
        auto has_prev = memb.type.prevcond.size() != 0 && !check_after;
        auto has_exist = memb.type.existcond.size() != 0;
        auto has_after = memb.type.prevcond.size() != 0 && check_after;
        auto cond_begin_one = [&](auto& m, bool not_) {
            hlp::appends(str, "if (");
            if (not_) {
                hlp::append(str, "!(");
            }
            render_tree(str, m.tree, out);
            if (not_) {
                hlp::append(str, ")");
            }
            hlp::appends(str, "){\n");
        };
        auto cond_begin = [&](auto& cond, bool not_) {
            for (auto& m : cond) {
                cond_begin_one(m, not_);
            }
        };
        auto cond_end = [&](auto& cond) {
            for (auto& m : cond) {
                hlp::append(str, "}\n");
            }
        };
        if (has_prev) {
            for (auto& m : memb.type.prevcond) {
                cond_begin_one(m, true);
                hlp::append(str, "return ");
                render_cpp_errorcode(str, st, &m, &memb, data);
                hlp::append(str, ";\n");
                hlp::append(str, "}\n");
            }
        }
        if (has_exist) {
            cond_begin(memb.type.existcond, false);
        }
        if (data.structs.find(memb.type.name) != data.structs.end()) {
            if (hlp::equal(coder, "decode")) {
                hlp::appends(str, "if(auto e__ =", coder, "(", target, ",", out, ".", memb.name, ");");
            }
            else {
                hlp::appends(str, "if(auto e__ =", coder, "(", out, ".", memb.name, ",", target, ");");
            }
            render_cpp_cond_err(str, st.errtype, data);
            hlp::append(str, "){\n");
            hlp::append(str, "return e__;\n");
            hlp::append(str, "}\n");
        }
        else {
            if (check_failed) {
                hlp::append(str, "if(!");
            }
            hlp::appends(str, target, ".", method, "(", out, ".", memb.name);
            if (memb.type.size) {
                hlp::append(str, ",");
                render_tree(str, memb.type.size, out);
            }
            if (check_failed) {
                hlp::append(str, ")){\n");
                hlp::append(str, "return ");
                render_cpp_errorcode(str, st, nullptr, &memb, data);
                hlp::append(str, ";\n");
                hlp::append(str, "}\n");
            }
            else {
                hlp::append(str, ");\n");
            }
        }
        if (has_after) {
            for (Cond& m : memb.type.prevcond) {
                cond_begin_one(m, true);
                hlp::append(str, "return ");
                render_cpp_errorcode(str, st, &m, &memb, data);
                hlp::append(str, ";\n");
                hlp::append(str, "}\n");
            }
        }
        if (has_exist) {
            cond_end(memb.type.existcond);
        }
    }

    void generate_ptr_obj(utw::string& str, FileData& data, auto& obj) {
        if (data.ptr_type.size()) {
            hlp::appends(str, data.ptr_type, "<", obj, ">");
        }
        else {
            hlp::appends(str, obj, "*");
        }
    }

    void generate_make_ptr_obj(utw::string& str, FileData& data, auto& obj) {
        if (data.make_ptr.size()) {
            hlp::appends(str, data.make_ptr, "<", obj, ">()");
        }
        else {
            hlp::appends(str, "new ", obj, "{}");
        }
    }

    void generate_delete_ptr_obj(utw::string& str, FileData& data, auto& obj) {
        if (!data.make_ptr.size()) {
            hlp::appends(str, "delete ", obj, ";");
        }
    }

    bool compare_tree(tree_t& left, tree_t& right) {
        if (left == right) {
            return true;
        }
        if (!left || !right) {
            return false;
        }
        if (!compare_tree(left->left, right->left) || !compare_tree(left->right, right->left)) {
            return false;
        }
        if (left->kw != right->kw) {
            return false;
        }
        if (left->token != right->token) {
            return false;
        }
        return true;
    }

    using Dependency = utw::map<utw::string, utw::vector<utw::string>>;
    namespace us = utils::syntax;
    void generate_dependency(Dependency& dep, utw::string& str, FileData& data, GenFlag flag) {
        for (auto& d : dep) {
            if (d.second.size() <= 1) {
                continue;
            }
            utw::string depparam;
            FlagType type = FlagType::none;
            bool first = true;
            bool not_match = false;
            tree_t cond;
            auto& bsst = *data.structs.find(d.first);
            utw::string& errtype = bsst.second.errtype;
            for (auto& i : d.second) {
                auto& st = *data.structs.find(i);
                if (errtype != st.second.errtype) {
                    not_match = true;
                    break;
                }
                if (!st.second.base.type.existcond.size()) {
                    not_match = true;
                    break;
                }
                if (first) {
                    cond = st.second.base.type.existcond[0].tree;
                    if (!cond->right || !cond->left) {
                        not_match = true;
                        break;
                    }
                }
                else {
                    auto cmp = st.second.base.type.existcond[0];
                    if (!cmp.tree->right || !cmp.tree->left) {
                        not_match = true;
                        break;
                    }
                    if (!compare_tree(cmp.tree->left, cond->left) || compare_tree(cmp.tree->right, cond->right)) {
                        not_match = true;
                        break;
                    }
                }
                first = false;
            }
            if (not_match) {
                continue;
            }
            hlp::appends(str,
                         "template<class Input>\n",
                         bsst.second.errtype, " decode(Input&& input,");
            generate_ptr_obj(str, data, d.first);
            hlp::append(str, "& output) {\n");
            write_indent(str, 1);
            hlp::appends(str, d.first, " judgement;\n");
            write_indent(str, 1);
            hlp::append(str, "if (auto e__ = decode(input,judgement);");
            render_cpp_cond_err(str, bsst.second.errtype, data);
            hlp::append(str, ") {\n");
            write_indent(str, 2);
            hlp::append(str, "return e__;\n");
            write_indent(str, 1);
            hlp::append(str, "}\n");
            write_indent(str, 1);
            auto gen_decode = [&](auto& st) {
                auto cond = st.second.base.type.existcond[0];
                hlp::append(str, "if(");
                render_tree(str, cond.tree, "judgement");
                hlp::append(str, "){\n");
                write_indent(str, 1);
                hlp::appends(str, "auto p = ");
                generate_make_ptr_obj(str, data, st.first);
                hlp::append(str, ";\n");
                for (auto& memb : bsst.second.member) {
                    write_indent(str, 2);
                    hlp::appends(str, "p->", memb.name, " = std::move(judgement.", memb.name, ");\n");
                }
                write_indent(str, 2);
                hlp::append(str, "if(auto e__ = decode(input,*p,true);");
                render_cpp_cond_err(str, st.second.errtype, data);
                hlp::append(str, ") {\n");
                write_indent(str, 3);
                generate_delete_ptr_obj(str, data, "p");
                hlp::append(str, "\n");
                write_indent(str, 3);
                hlp::append(str, "return e__;\n");
                write_indent(str, 2);
                hlp::append(str, "}\n");
                write_indent(str, 2);
                hlp::append(str, "output=p;\n");
                write_indent(str, 2);
                hlp::append(str, "return ");
                render_cpp_succeed_res(str, st.second.errtype, data);
                hlp::append(str, ";\n");
                write_indent(str, 1);
                hlp::append(str, "}\n");
                write_indent(str, 1);
                hlp::append(str, "else ");
            };
            for (auto& id : d.second) {
                gen_decode(*data.structs.find(id));
            }
            hlp::append(str, "{\n");
            write_indent(str, 2);
            hlp::append(str, "return ");
            render_cpp_errorcode(str, bsst.second, nullptr, nullptr, data);
            hlp::append(str, ";\n");
            write_indent(str, 1);
            hlp::append(str, "}\n}\n\n");
        }
    }
    void render_cpp_namespace_begin(utw::string& str, auto&& name) {
        hlp::append(str, "namespace ");
        hlp::append(str, name);
        hlp::append(str, " {\n");
    }

    void render_cpp_namespace_end(utw::string& str, auto&& name) {
        hlp::appends(str, "} // namespace ", name, "\n");
    }

    void generate_cpp(utw::string& str, FileData& data, GenFlag flag) {
        if (any(flag & GenFlag::add_license)) {
            hlp::append(str, "/*license*/\n");
        }
        hlp::appends(str, "// Code generated by binred (https://github.com/on-keyday/utils)\n\n",
                     "#pragma once\n");
        for (auto& i : data.imports) {
            hlp::appends(str, "#include", i, "\n");
        }
        hlp::append(str, "\n");
        Dependency dependency;
        if (data.pkgname.size()) {
            bool viewed = false;
            if (any(flag & GenFlag::sep_namespace)) {
                auto spltview = hlp::make_ref_splitview(data.pkgname, "::");
                auto sz = spltview.size();
                for (auto i = 0; i < sz; i++) {
                    render_cpp_namespace_begin(str, spltview[i]);
                    viewed = true;
                }
            }
            if (!viewed) {
                render_cpp_namespace_begin(str, data.pkgname);
            }
        }
        for (auto& def : data.defvec) {
            auto& d = *data.structs.find(def);
            auto& st = d.second;
            hlp::appends(str, "struct ", d.first);
            auto has_base = st.base.type.name.size() != 0;
            if (has_base) {
                hlp::appends(str, " : ", st.base.type.name);
                dependency[st.base.type.name].push_back(d.first);
            }
            hlp::append(str, " {\n");
            for (auto& memb : st.member) {
                hlp::appends(str, "    ", memb.type.name, " ", memb.name);
                if (memb.defval.size()) {
                    hlp::appends(str, " = ", memb.defval);
                }
                hlp::append(str, ";\n");
            }
            hlp::append(str, "};\n\n");
            if (!st.errtype.size()) {
                st.errtype = "bool";
            }
            hlp::appends(str,
                         "template<class Output>\n",
                         st.errtype, " encode(const ", d.first, "& input,Output& output){\n");
            auto gen_base_flag = [&](auto& flag, auto& io) {
                for (Cond& m : flag) {
                    hlp::append(str, "if(!(");
                    render_tree(str, m.tree, io);
                    hlp::append(str, ")){\n");
                    hlp::appends(str, "return ");
                    render_cpp_errorcode(str, st, &m, nullptr, data);
                    hlp::appends(str, ";\n", "}\n");
                }
            };
            if (has_base) {
                gen_base_flag(st.base.type.prevcond, "input");
                gen_base_flag(st.base.type.existcond, "input");
                write_indent(str, 1);
                hlp::appends(str, "if (auto e__ = encode(static_cast<const ", st.base.type.name, "&>(input),output);");
                render_cpp_cond_err(str, st.errtype, data);
                hlp::appends(str, ") { \n");
                write_indent(str, 2);
                hlp::appends(str, "return e__;\n");
                write_indent(str, 1);
                hlp::append(str, "}\n");
                // gen_base_flag(st.base.type.aftercond, "input");
            }
            for (auto& memb : st.member) {
                generate_with_cond(data, str, st, memb, "output", data.write_method, "input", "encode", false, false);
                // generate_with_flag(data, str, memb, "input", "output", data.write_method, true, false);
            }
            write_indent(str, 1);
            hlp::append(str, "return ");
            render_cpp_succeed_res(str, st.errtype, data);
            hlp::append(str, ";\n");
            hlp::append(str, "}\n\n");
            hlp::appends(str, "template<class Input>\n",
                         st.errtype, " decode(Input&& input,",
                         d.first, "& output");
            if (has_base) {
                hlp::append(str, ",bool base_set=false");
            }
            hlp::append(str, "){\n");
            if (has_base) {
                int offset = 1;
                write_indent(str, offset);
                hlp::appends(str, "if (!base_set) {\n");
                write_indent(str, offset + 1);
                hlp::appends(str, "if (auto e__ = decode(input,static_cast<", st.base.type.name, "&>(output));");
                render_cpp_cond_err(str, st.errtype, data);
                hlp::append(str, ") { \n");
                write_indent(str, offset + 2);
                hlp::append(str, "return e__;\n");
                write_indent(str, offset + 1);
                hlp::append(str, "}\n");
                gen_base_flag(st.base.type.existcond, "output");
                write_indent(str, offset);
                hlp::appends(str, "}\n");
                gen_base_flag(st.base.type.prevcond, "output");
            }
            for (auto& memb : st.member) {
                generate_with_cond(data, str, st, memb, "input", data.read_method, "output", "decode", true, true);
                // generate_with_flag(data, str, memb, "output", "input", data.read_method, false, true);
            }
            write_indent(str, 1);
            hlp::append(str, "return ");
            render_cpp_succeed_res(str, st.errtype, data);
            hlp::append(str, ";\n");
            hlp::append(str, "}\n\n");
        }
        generate_dependency(dependency, str, data, flag);
        if (data.pkgname.size()) {
            bool viewed = false;
            if (any(flag & GenFlag::sep_namespace)) {
                auto spltview = hlp::make_ref_splitview(data.pkgname, "::");
                auto sz = spltview.size();
                for (auto i = sz - 1; i != ~0; i--) {
                    render_cpp_namespace_end(str, spltview[i]);
                    viewed = true;
                }
            }
            if (!viewed) {
                render_cpp_namespace_end(str, data.pkgname);
            }
        }
    }
}  // namespace binred
