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

    void render_cpp_noref_type(Type& type, utw::string& str) {
        if (type.is_const) {
            hlp::append(str, "const ");
        }
        hlp::append(str, type.prim);
        for (size_t i = 0; i < type.pointer; i++) {
            hlp::append(str, "*");
        }
    }

    void render_cpp_type(Type& type, utw::string& str) {
        render_cpp_noref_type(type, str);
        if (type.ref == RefKind::lval) {
            hlp::append(str, "&");
        }
        else if (type.ref == RefKind::rval) {
            hlp::append(str, "&&");
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
            if (arg.type.ref == RefKind::rval) {
                hlp::append(str, "std::forward<");
                render_cpp_noref_type(arg.type, str);
                hlp::append(str, ">(");
            }
            hlp::append(str, arg.name);
            if (arg.type.ref == RefKind::rval) {
                hlp::append(str, ")");
            }
        }
        hlp::append(str, ")");
    }

    void render_cpp_default_value(Interface& func, utw::string& str) {
        if (func.default_result.size()) {
            if (func.default_result == "panic") {
                hlp::append(str, "throw std::bad_function_call()");
            }
            else {
                hlp::append(str, func.default_result);
            }
        }
        else {
            if (func.type.ref != RefKind::none) {
                hlp::append(str, "throw std::bad_function_call()");
            }
            else if (func.type.pointer) {
                hlp::append(str, "nullptr");
            }
            else {
                hlp::append(str, func.type.prim);
                hlp::append(str, "{}");
            }
        }
    }

    bool generate_cpp(FileData& data, utw::string& str) {
        hlp::append(str, R"(
// Code generated by ifacegen (https://github.com/on-keyday/utils) DO NOT EDIT.

#pragma once
#include<helper/deref.h>
)");
        for (auto& h : data.headernames) {
            hlp::append(str, "#include");
            hlp::append(str, h);
            hlp::append(str, "\n");
        }
        hlp::append(str, "\n");
        if (data.pkgname.size()) {
            hlp::append(str, "namespace ");
            hlp::append(str, data.pkgname);
            hlp::append(str, " {\n");
        }
        for (auto& iface : data.ifaces) {
            hlp::append(str, "struct ");
            hlp::append(str, iface.first);
            hlp::append(str, " {");
            hlp::append(str, R"(
   private:
    struct interface {
    )");
            for (auto& func : iface.second) {
                hlp::append(str, "    virtual ");
                render_cpp_function(func, str);
                hlp::append(str, "= 0;\n    ");
            }
            hlp::append(str, R"(
        virtual ~interface(){}
    };
    
    template<class T>
    struct implement : interface {
        T t_holder_;

        template<class... Args>
        implement(Args&&...args)
            :t_holder_(std::forward<Args>(args)...){}

    )");
            for (auto& func : iface.second) {
                hlp::append(str, "    ");
                render_cpp_function(func, str);
                hlp::append(str, "override {\n");
                hlp::append(str, "            ");
                hlp::append(str, "auto t_ptr_ = utils::helper::deref(this->t_holder_);\n");
                hlp::append(str, "            ");
                hlp::append(str, "if (!t_ptr_) {\n");
                hlp::append(str, "                ");
                hlp::append(str, "return ");
                render_cpp_default_value(func, str);
                hlp::append(str, ";\n");
                hlp::append(str, "            }\n");
                hlp::append(str, "            ");
                hlp::append(str, "return t_ptr_->");
                render_cpp_call(func, str);
                hlp::append(str, ";\n        }\n\n    ");
            }
            hlp::append(str, R"(};

    interface* iface = nullptr;

   public:
    constexpr )");
            hlp::append(str, iface.first);
            hlp::append(str, "(){}\n\n    constexpr ");
            hlp::append(str, iface.first);
            hlp::append(str, "(std::nullptr_t){}\n");
            hlp::append(str, R"(
    template <class T>
    )");
            hlp::append(str, iface.first);
            hlp::append(str, R"a((T&& t) {
        iface=new implement<std::decay_t<T>>(std::forward<T>(t));
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

    operator bool() const {
        return iface != nullptr;
    }

)");
            for (auto& func : iface.second) {
                hlp::append(str, "    ");
                render_cpp_function(func, str);
                hlp::append(str, R"({
        return iface?iface->)");
                render_cpp_call(func, str);
                hlp::append(str, ":");
                render_cpp_default_value(func, str);
                hlp::append(str, R"(;
    }

)");
            }
            hlp::append(str, "};\n\n");
        }
        if (data.pkgname.size()) {
            hlp::append(str, "} // namespace ");
            hlp::append(str, data.pkgname);
            hlp::append(str, "\n");
        }
        return true;
    }

    bool generate(FileData& data, utw::string& str, Language lang) {
        if (lang == Language::cpp) {
            return generate_cpp(data, str);
        }
        return false;
    }
}  // namespace ifacegen
