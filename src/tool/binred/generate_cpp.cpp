/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "type_list.h"

#include "../../include/helper/appender.h"

namespace binred {
    namespace hlp = utils::helper;

    void write_indent(utw::string& str, size_t indent) {
        for (size_t i = 0; i < indent; i++) {
            hlp::append(str, "    ");
        }
    }

    void generate_flag_cond_begin(utw::string& str, auto& in, Flag& flag, bool not_ = false) {
        hlp::append(str, "if (");
        if (not_) {
            hlp::append(str, "!(");
        }
        hlp::appends(str, in, ".", flag.depend);
        if (flag.type == FlagType::eq) {
            hlp::append(str, " == ");
        }
        else if (flag.type == FlagType::bit) {
            hlp::append(str, " & ");
        }
        else if (flag.type == FlagType::ls) {
            hlp::append(str, " < ");
        }
        else if (flag.type == FlagType::gt) {
            hlp::append(str, " > ");
        }
        else if (flag.type == FlagType::egt) {
            hlp::append(str, " >= ");
        }
        else if (flag.type == FlagType::els) {
            hlp::append(str, " <=");
        }
        else if (flag.type == FlagType::nq) {
            hlp::append(str, " != ");
        }
        hlp::append(str, flag.val.val);
        if (not_) {
            hlp::append(str, ")");
        }
        hlp::append(str, ") {\n");
        write_indent(str, 1);
    }

    void generate_with_flag(utw::string& str, Member& memb, auto& in, auto& out, auto& method, bool check_succeed) {
        auto& flag = memb.type.flag;
        auto has_flag = flag.type != FlagType::none;
        auto check_before = memb.type.flag.depend != memb.name;
        int plus = 0;
        auto check_self = [&] {
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
                generate_flag_cond_begin(str, in, flag);
                plus = 1;
            }
            else if (!check_succeed) {
                check_self();
            }
        }
        write_indent(str, 1);
        if (check_succeed) {
            hlp::append(str, "if(!");
        }
        hlp::appends(str, out, ".", method, "(", in, ".", memb.name);
        if (memb.type.flag.size.val.size()) {
            hlp::append(str, ",");
            if (memb.type.flag.size.kind == utils::syntax::KeyWord::id) {
                hlp::appends(str, in, ".");
            }
            hlp::appends(str, memb.type.flag.size.val);
        }
        hlp::append(str, ")");
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
                check_self();
            }
        }
    }

    void generate_ptr_obj(utw::string& str, FileData& data, auto& obj) {
        hlp::appends(str, obj, "*");
    }

    void generate_make_ptr_obj(utw::string& str, FileData& data, auto& obj) {
        hlp::appends(str, "new ", obj, "{}");
    }

    void generate_delete_ptr_obj(utw::string& str, FileData& data, auto& obj) {
        hlp::appends(str, "delete ", obj, ";");
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
                auto& flag = st.second.base.type.flag;
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
                generate_flag_cond_begin(str, "judgement", st.second.base.type.flag);
                write_indent(str, 1);
                hlp::appends(str, "auto p = ");
                generate_make_ptr_obj(str, data, st.first);
                hlp::append(str, ";\n");
                for (auto& memb : bsst.second.member) {
                    write_indent(str, 2);
                    hlp::appends(str, "p->", memb.name, " = std::move(judgement.", memb.name, ");\n");
                }
                write_indent(str, 2);
                hlp::append(str, "if(decode(input,*p)) {\n");
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
        hlp::appends(str, "// Code generated by binred (https://github.com/on-keyday/utils)\n",
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
            if (has_base) {
                if (st.base.type.flag.type != FlagType::none) {
                    write_indent(str, 1);
                    generate_flag_cond_begin(str, "input", st.base.type.flag, true);
                    write_indent(str, 1);
                    hlp::append(str, "return false;\n");
                    write_indent(str, 1);
                    hlp::append(str, "}\n");
                }
                write_indent(str, 1);
                hlp::appends(str, "if (!encode(static_cast<const ", st.base.type.name, "&>(input),output)) { \n");
                write_indent(str, 2);
                hlp::appends(str, "return false;\n");
                write_indent(str, 1);
                hlp::append(str, "}\n");
            }
            for (auto& memb : d.second.member) {
                generate_with_flag(str, memb, "input", "output", data.write_method, false);
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
                if (st.base.type.flag.type != FlagType::none) {
                    write_indent(str, 1);
                    generate_flag_cond_begin(str, "output", st.base.type.flag, true);
                    write_indent(str, offset);
                    hlp::append(str, "return false;\n");
                    write_indent(str, offset);
                    hlp::append(str, "}\n");
                }
            }
            for (auto& memb : d.second.member) {
                generate_with_flag(str, memb, "output", "input", data.read_method, true);
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
