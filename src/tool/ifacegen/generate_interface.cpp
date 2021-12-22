/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "interface_list.h"
#include "../../include/helper/appender.h"

namespace ifacegen {
    namespace hlp = utils::helper;

    void render_cpp_type(Type& type, utw::string& str) {
        if (type.is_const) {
            hlp::append(str, "const ");
        }
        hlp::append(str, type.prim);
        for (size_t i = 0; i < type.pointer; i++) {
            hlp::append(str, "*");
        }
        hlp::append(str, " ");
    }

    void render_cpp_function(Interface& func, utw::string& str) {
        render_cpp_type(func.type, str);
        hlp::append(str, func.funcname);
        hlp::append(str, "(");
        bool is_first = true;
        for (auto& arg : func.args) {
            if (!is_first) {
                hlp::append(str, ", ");
            }
            render_cpp_type(arg.type, str);
            hlp::append(str, arg.name);
            is_first = false;
        }
        hlp::append(str, ") ");
        if (func.is_const) {
            hlp::append(str, "const ");
        }
    }

    void render_cpp_call(Interface& func, utw::string& str) {
        hlp::append(str, func.funcname);
        hlp::append(str, "(");
        bool is_first = true;
        for (auto& arg : func.args) {
            if (!is_first) {
                hlp::append(str, ", ");
            }
            hlp::append(str, arg.name);
        }
        hlp::append(str, ");");
    }

    bool generate_cpp(FileData& data, utw::string& str) {
        hlp::append(str, R"(
// Code generated by ifacegen (https://github.com/on-keyday/utils) DO NOT EDIT.

#pragma once
#include<helper/sfinae.h>
#include<helper/deref.h>
)");
        if (data.pkgname.size()) {
            hlp::append(str, "namespace ");
            hlp::append(str, data.pkgname);
            hlp::append(str, " {\n");
        }
        for (auto& iface : data.ifaces) {
            hlp::append(str, "    struct ");
            hlp::append(str, iface.first);
            hlp::append(str, " {\n");
            hlp::append(str, R"(
   private:
    struct interface {
    )");
            for (auto& func : iface.second) {
                hlp::append(str, "    virtual ");
                render_cpp_function(func, str);
                hlp::append(str, "= 0;\n    ");
            }
            hlp::append(str, R"(};
    
    template<class T>
    struct implement : interface {
        T t_holder_;
        template<class... Args>
        implement(Args&&...args)
            :t(std::forward<Args>(args)...){}
    )");
            for (auto& func : iface.second) {
                hlp::append(str, "    ");
                render_cpp_function(func, str);
                hlp::append(str, "override {\n");
                hlp::append(str, "            ");
                hlp::append(str, "auto t_ptr_ = utils::deref(this->t_holder_)\n");
                hlp::append(str, "            ");
                hlp::append(str, "if (!t_ptr_) {\n");
                hlp::append(str, "                ");
                hlp::append(str, "return ");
                if (func.default_result.size()) {
                    hlp::append(str, func.default_result);
                }
                else {
                    hlp::append(str, "{}");
                }
                hlp::append(str, ";\n");
                hlp::append(str, "            }\n");
                hlp::append(str, "            ");
                hlp::append(str, "return t_ptr_->");
                render_cpp_call(func, str);
                hlp::append(str, "\n    }\n    ");
            }
            hlp::append(str, R"(};
    interface* iface = nullptr;
   public:
    template<class T>
    )");
            hlp::append(str, iface.first);
            hlp::append(str, R"a((T&& t) {
        iface=new implement<std::decay_t<T>>(std::forward<T>(t))
    }
    
    )a");
            hlp::append(str, iface.first);
            hlp::append(str, "(");
            hlp::append(str, iface.first);
            hlp::append(str, R"(&& in) {
        iface=in.iface;
        in.iface=nullptr;
    }
    
    )");
            hlp::append(str, iface.first);
            hlp::append(str, "& operator=(");
            hlp::append(str, iface.first);
            hlp::append(str, R"(&& in) {
        delete iface;
        iface=in.iface;
        in.iface=nullptr;
        return *this;
    }

    )");
        }
    }

    bool generate(FileData& data, utw::string& str, Language lang) {
        if (lang == Language::cpp) {
            return generate_cpp(data, str);
        }
        return false;
    }
}  // namespace ifacegen