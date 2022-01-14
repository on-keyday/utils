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
            if (rel.size() && str.back() != '.') {
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
    /*
    void set_size(utw::string& str, Size& size, auto& in) {
        set_val(str, size.size1, in);
        if (size.op != Op::none) {
            if (size.op == Op::add) {
                hlp::append(str, " + ");
            }
            else if (size.op == Op::sub) {
                hlp::append(str, " - ");
            }
            else if (size.op == Op::mod) {
                hlp::append(str, " % ");
            }
            set_val(str, size.size2, in);
        }
    }*/

    void generate_with_cond(FileData& data, utw::string& str, Member& memb, auto& target,
                            auto& method, auto& out, auto& coder, bool check_after, bool check_failed) {
        auto has_prev = memb.type.prevcond.size() != 0 && !check_after;
        auto has_exist = memb.type.existcond.size() != 0;
        auto has_after = memb.type.prevcond.size() != 0 && check_after;
        auto cond_begin_one = [&](auto& m, bool not_) {
            hlp::appends(str, "if (");
            if (not_) {
                hlp::append(str, "!(");
            }
            render_tree(str, m, out);
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
                hlp::append(str, "return false;\n");
                hlp::append(str, "}\n");
            }
        }
        if (has_exist) {
            cond_begin(memb.type.existcond, false);
        }
        if (data.structs.find(memb.type.name) != data.structs.end()) {
            if (hlp::equal(coder, "decode")) {
                hlp::appends(str, "if(!", coder, "(", target, ",", out, ".", memb.name, ")){\n");
            }
            else {
                hlp::appends(str, "if(!", coder, "(", out, ".", memb.name, ",", target, ")){\n");
            }
            hlp::append(str, "return false;\n");
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
                hlp::append(str, "return false;\n");
                hlp::append(str, "}\n");
            }
            else {
                hlp::append(str, ");\n");
            }
        }
        if (has_after) {
            for (auto& m : memb.type.prevcond) {
                cond_begin_one(m, true);
                hlp::append(str, "return false;\n");
                hlp::append(str, "}\n");
            }
        }
        if (has_exist) {
            cond_end(memb.type.existcond);
        }
    }

    /*
    void generate_with_flag(FileData& data, utw::string& str, Member& memb, auto& in, auto& out, auto& method, bool is_enc, bool check_succeed) {
        auto& type = memb.type;
        //auto has_flag = type.flag.type != FlagType::none;
        //auto has_bind = type.bind.type != FlagType::none;
        auto check_before = memb.type.flag.depend != memb.name;
        int plus = 0;
        auto check_self = [&](auto& flag) {
            write_indent(str, 1);
            generate_flag_cond_begin(str, in, flag, true);
            write_indent(str, 1);
            hlp::append(str, "return false;\n");
            write_indent(str, 1);
            hlp::append(str, "}\n");
        };
        if (has_flag) {
            if (check_before) {
                write_indent(str, 1);
                generate_flag_cond_begin(str, in, type.flag);
                plus = 1;
            }
            else if (!check_succeed) {
                check_self(type.flag);
            }
        }
        if (has_bind && !check_succeed) {
            check_self(type.bind);
        }
        write_indent(str, 1);
        if (check_succeed) {
            hlp::append(str, "if(!");
        }
        if (data.structs.count(memb.type.name)) {
            if (is_enc) {
                hlp::appends(str, "encode(", in, ".", memb.name, ", ", out, ")");
            }
            else {
                hlp::appends(str, "decode(", in, ", ", out, ".", memb.name, ")");
            }
        }
        else {
            hlp::appends(str, out, ".", method, "(", in, ".", memb.name);
            if (memb.type.flag.size.size1.val.size()) {
                hlp::append(str, ",");
                set_size(str, memb.type.flag.size, in);
            }
            hlp::append(str, ")");
        }
        if (check_succeed) {
            hlp::append(str, ") {\n");
            write_indent(str, 2 + plus);
            hlp::append(str, "return false;\n");
            write_indent(str, 1 + plus);
            hlp::append(str, "}\n");
        }
        else {
            hlp::append(str, ";\n");
        }
        if (has_flag) {
            if (check_before) {
                write_indent(str, 1);
                hlp::append(str, "}\n");
            }
            else if (check_succeed) {
                check_self(type.flag);
            }
        }
        if (has_bind && check_succeed) {
            check_self(type.bind);
        }
    }
*/
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
            auto& bsst = *data.structs.find(d.first);
            for (auto& i : d.second) {
                auto& st = *data.structs.find(i);
                auto flag = Flag{};
                if (first) {
                    if (flag.type == FlagType::none) {
                        not_match = true;
                        break;
                    }
                    depparam = flag.depend;
                    type = flag.type;
                    first = false;
                }
                else {
                    if (flag.depend != depparam || flag.type != type) {
                        not_match = true;
                        break;
                    }
                }
            }
            if (not_match) {
                continue;
            }
            hlp::appends(str, "template<class Input>\nbool decode(Input&& input,");
            generate_ptr_obj(str, data, d.first);
            hlp::append(str, "& output) {\n");
            write_indent(str, 1);
            hlp::appends(str, d.first, " judgement;\n");
            write_indent(str, 1);
            hlp::append(str, "if (!decode(input,judgement)) {\n");
            write_indent(str, 2);
            hlp::append(str, "return false;\n");
            write_indent(str, 1);
            hlp::append(str, "}\n");
            write_indent(str, 1);
            auto gen_decode = [&](auto& st) {
                //generate_flag_cond_begin(str, "judgement", st.second.base.type.prevcond);
                write_indent(str, 1);
                hlp::appends(str, "auto p = ");
                generate_make_ptr_obj(str, data, st.first);
                hlp::append(str, ";\n");
                for (auto& memb : bsst.second.member) {
                    write_indent(str, 2);
                    hlp::appends(str, "p->", memb.name, " = std::move(judgement.", memb.name, ");\n");
                }
                write_indent(str, 2);
                hlp::append(str, "if(decode(input,*p,true)) {\n");
                write_indent(str, 3);
                generate_delete_ptr_obj(str, data, "p");
                hlp::append(str, "\n");
                write_indent(str, 3);
                hlp::append(str, "return false;\n");
                write_indent(str, 2);
                hlp::append(str, "}\n");
                write_indent(str, 2);
                hlp::append(str, "output=p;\n");
                write_indent(str, 2);
                hlp::append(str, "return true;\n");
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
            hlp::append(str, "return false;\n");
            write_indent(str, 1);
            hlp::append(str, "}\n}\n\n");
        }
    }

    void generate_cpp(utw::string& str, FileData& data, GenFlag flag) {
        hlp::appends(str, "// Code generated by binred (https://github.com/on-keyday/utils)\n\n",
                     "#pragma once\n");
        for (auto& i : data.imports) {
            hlp::appends(str, "#include", i, "\n");
        }
        hlp::append(str, "\n");
        Dependency dependency;
        if (data.pkgname.size()) {
            hlp::appends(str, "namespace ", data.pkgname, " {\n\n");
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
            hlp::appends(str, "template<class Output>\nbool encode(const ", d.first, "& input,Output& output){\n");
            auto gen_base_flag = [&](auto& flag, auto& io) {
                for (auto& m : flag) {
                    hlp::append(str, "if(!(");
                    render_tree(str, m, io);
                    hlp::append(str, ")){\n");
                    hlp::append(str, "return false;\n");
                    hlp::append(str, "}\n");
                }
            };
            if (has_base) {
                gen_base_flag(st.base.type.prevcond, "input");
                write_indent(str, 1);
                hlp::appends(str, "if (!encode(static_cast<const ", st.base.type.name, "&>(input),output)) { \n");
                write_indent(str, 2);
                hlp::appends(str, "return false;\n");
                write_indent(str, 1);
                hlp::append(str, "}\n");
                // gen_base_flag(st.base.type.aftercond, "input");
            }
            for (auto& memb : d.second.member) {
                generate_with_cond(data, str, memb, "output", data.write_method, "input", "encode", false, false);
                //generate_with_flag(data, str, memb, "input", "output", data.write_method, true, false);
            }
            write_indent(str, 1);
            hlp::append(str, "return true;\n");
            hlp::append(str, "}\n\n");
            hlp::appends(str, "template<class Input>\nbool decode(Input&& input,", d.first, "& output");
            if (has_base) {
                hlp::append(str, ",bool base_set=false");
            }
            hlp::append(str, "){\n");
            if (has_base) {
                int offset = 1;
                write_indent(str, offset);
                hlp::appends(str, "if (!base_set) {\n");
                write_indent(str, offset + 1);
                hlp::appends(str, "if (!decode(input,static_cast<", st.base.type.name, "&>(output))) { \n");
                write_indent(str, offset + 2);
                hlp::append(str, "return false;\n");
                write_indent(str, offset + 1);
                hlp::append(str, "}\n");
                write_indent(str, offset);
                hlp::appends(str, "}\n");
                gen_base_flag(st.base.type.prevcond, "output");
                //gen_base_flag(st.base.type.aftercond, "output");
            }
            for (auto& memb : d.second.member) {
                generate_with_cond(data, str, memb, "input", data.read_method, "output", "decode", true, true);
                //generate_with_flag(data, str, memb, "output", "input", data.read_method, false, true);
            }
            write_indent(str, 1);
            hlp::append(str, "return true;\n");
            hlp::append(str, "}\n\n");
        }
        generate_dependency(dependency, str, data, flag);
        if (data.pkgname.size()) {
            hlp::appends(str, "} // namespace ", data.pkgname, "\n");
        }
    }
}  // namespace binred
