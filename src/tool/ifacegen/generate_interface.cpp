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

    constexpr auto decltype_func = "decltype";
    constexpr auto copy_func = "__copy__";
    constexpr auto call_func = "__call__";

    void resolve_alias(utw::string& str, utw::string& prim, utw::map<utw::string, utw::string>* alias) {
        if (alias) {
            auto found = alias->find(prim);
            if (found != alias->end()) {
                hlp::append(str, found->second);
            }
            else {
                hlp::append(str, prim);
            }
        }
        else {
            hlp::append(str, prim);
        }
    }

    void render_cpp_noref_type(Type& type, utw::string& str, utw::map<utw::string, utw::string>* alias) {
        if (type.is_const) {
            hlp::append(str, "const ");
        }
        resolve_alias(str, type.prim, alias);
        for (size_t i = 0; i < type.pointer; i++) {
            hlp::append(str, "*");
        }
    }

    void render_cpp_type(Type& type, utw::string& str, utw::map<utw::string, utw::string>* alias) {
        render_cpp_noref_type(type, str, alias);
        if (type.ref == RefKind::lval) {
            hlp::append(str, "&");
        }
        else if (type.ref == RefKind::rval) {
            hlp::append(str, "&&");
        }
        hlp::append(str, " ");
    }

    void render_cpp_function(Interface& func, utw::string& str, utw::map<utw::string, utw::string>* alias) {
        render_cpp_type(func.type, str, alias);
        if (func.funcname == "__call__") {
            hlp::append(str, "operator()");
        }
        else {
            hlp::append(str, func.funcname);
        }
        hlp::append(str, "(");
        bool is_first = true;
        for (auto& arg : func.args) {
            if (!is_first) {
                hlp::append(str, ", ");
            }
            render_cpp_type(arg.type, str, alias);
            hlp::append(str, arg.name);
            is_first = false;
        }
        hlp::append(str, ") ");
        if (func.is_const) {
            hlp::append(str, "const ");
        }
    }

    void render_cpp_call(Interface& func, utw::string& str, utw::map<utw::string, utw::string>* alias) {
        if (func.funcname == call_func) {
            //hlp::append(str, "operator()");
        }
        else {
            hlp::append(str, func.funcname);
        }
        hlp::append(str, "(");
        bool is_first = true;
        for (auto& arg : func.args) {
            if (!is_first) {
                hlp::append(str, ", ");
            }
            if (arg.type.ref == RefKind::rval) {
                hlp::append(str, "std::forward<");
                render_cpp_noref_type(arg.type, str, alias);
                hlp::append(str, ">(");
            }
            hlp::append(str, arg.name);
            if (arg.type.ref == RefKind::rval) {
                hlp::append(str, ")");
            }
            is_first = false;
        }
        hlp::append(str, ")");
    }

    void render_cpp_default_value(Interface& func, utw::string& str, bool need_ret, utw::map<utw::string, utw::string>* alias) {
        auto ret_w = [&] {
            if (need_ret) {
                hlp::append(str, "return ");
            }
        };
        if (func.default_result.size()) {
            if (func.default_result == "panic") {
                hlp::append(str, "throw std::bad_function_call()");
            }
            else {
                ret_w();
                hlp::append(str, func.default_result);
            }
        }
        else {
            if (func.type.ref != RefKind::none) {
                hlp::append(str, "throw std::bad_function_call()");
            }
            else if (func.type.pointer) {
                ret_w();
                hlp::append(str, "nullptr");
            }
            else if (func.type.prim == "void") {
                ret_w();
                hlp::append(str, "(void)0");
            }
            else {
                ret_w();
                resolve_alias(str, func.type.prim, alias);
                hlp::append(str, "{}");
            }
        }
    }

    bool generate_cpp(FileData& data, utw::string& str, GenFlag flag) {
        if (any(flag & GenFlag::add_license)) {
            hlp::append(str, "/*license*/\n");
        }
        hlp::append(str, R"(// Code generated by ifacegen (https://github.com/on-keyday/utils)

#pragma once
#include)");
        if (data.helper_deref.size()) {
            hlp::append(str, data.helper_deref);
        }
        else {
            hlp::append(str, "<helper/deref.h>");
        }
        hlp::append(str, "\n");
        auto has_other_typeinfo = data.typeid_func.size() && data.typeid_type.size();
        auto use_dycast = any(flag & GenFlag::use_dyn_cast);
        auto has_alloc = any(flag & GenFlag::use_allocator);
        if (has_alloc) {
            hlp::append(str, "#include<memory>\n");
        }
        if (data.has_ref_ret) {
            hlp::append(str, "#include<functional>\n");
        }
        for (auto& h : data.headernames) {
            hlp::append(str, "#include");
            hlp::append(str, h);
            hlp::append(str, "\n");
        }
        if (any(flag & GenFlag::no_vtable)) {
            hlp::append(str, R"(
#ifndef NOVTABLE__
#ifdef _WIN32
#define NOVTABLE__ __declspec(novtable)
#else
#define NOVTABLE__
#endif
#endif
)");
        }
        hlp::append(str, "\n");
        if (data.pkgname.size()) {
            hlp::append(str, "namespace ");
            hlp::append(str, data.pkgname);
            hlp::append(str, " {\n");
        }
        utw::map<utw::string, utw::string>* alias = nullptr;
        if (!any(flag & GenFlag::expand_alias)) {
            for (auto& alias : data.aliases) {
                hlp::append(str, "using ");
                hlp::append(str, alias.first);
                hlp::append(str, " = ");
                hlp::append(str, alias.second);
                hlp::append(str, ";\n");
            }
        }
        else {
            alias = &data.aliases;
        }
        for (auto& def : data.defvec) {
            auto& iface = *data.ifaces.find(def);
            if (has_alloc) {
                hlp::append(str, "template<class Alloc__ = std::allocator>\n");
            }
            hlp::append(str, "struct ");
            hlp::append(str, iface.first);
            hlp::append(str, " {");
            hlp::append(str, R"(
   private:

)");
            if (has_alloc) {
                hlp::append(str, "    Alloc__ alloc_obj__;\n\n");
            }
            hlp::append(str, "    struct ");
            if (any(flag & GenFlag::no_vtable)) {
                hlp::append(str, "NOVTABLE__ ");
            }
            hlp::append(str, R"(interface__ {
    )");
            for (auto& func : iface.second) {
                if (func.funcname == decltype_func) {
                    if (use_dycast) {
                        continue;
                    }
                    hlp::append(str, R"(    virtual const void* raw__() const = 0;
        virtual )");
                    if (has_other_typeinfo) {
                        hlp::append(str, data.typeid_type);
                    }
                    else {
                        hlp::append(str, "const std::type_info&");
                    }
                    hlp::append(str, R"( type__() const = 0;
    )");
                }
                else if (func.funcname == copy_func) {
                    hlp::append(str, "    virtual interface__* copy__() const = 0;\n    ");
                }
                else {
                    hlp::append(str, "    virtual ");
                    render_cpp_function(func, str, alias);
                    hlp::append(str, "= 0;\n    ");
                }
            }
            hlp::append(str, R"(
        virtual ~interface__(){}
    };
    
    template<class T>
    struct implements__ : interface__ {
        T t_holder_;

        template<class... Args>
        implements__(Args&&...args)
            :t_holder_(std::forward<Args>(args)...){}

    )");
            for (auto& func : iface.second) {
                if (func.funcname == decltype_func) {
                    if (use_dycast) {
                        continue;
                    }
                    hlp::append(str,
                                R"(    const void* raw__() const override {   
            return reinterpret_cast<const void*>(std::addressof(t_holder_));
        }
        
        )");
                    if (has_other_typeinfo) {
                        hlp::append(str, data.typeid_type);
                    }
                    else {
                        hlp::append(str, "const std::type_info&");
                    }
                    hlp::append(str, R"( type__() const override {
            return )");
                    if (has_other_typeinfo) {
                        hlp::append(str, data.typeid_func);
                    }
                    else {
                        hlp::append(str, "typeid(T)");
                    }
                    hlp::append(str, R"(;
        }

    )");
                }
                else if (func.funcname == copy_func) {
                    hlp::append(str, R"(    interface__* copy__() const override {
            )");

                    hlp::append(str, R"(return new implements__<T>(t_holder_);)");
                    hlp::append(str, R"(
                }

    )");
                }
                else {
                    hlp::append(str, "    ");
                    render_cpp_function(func, str, alias);
                    hlp::append(str, "override {\n");
                    hlp::append(str, "            ");
                    hlp::append(str, "auto t_ptr_ = utils::helper::deref(this->t_holder_);\n");
                    hlp::append(str, "            ");
                    hlp::append(str, "if (!t_ptr_) {\n");
                    hlp::append(str, "                ");
                    render_cpp_default_value(func, str, true, alias);
                    hlp::append(str, ";\n");
                    hlp::append(str, "            }\n");
                    hlp::append(str, "            ");
                    hlp::append(str, "return ");
                    if (func.funcname == "__call__") {
                        hlp::append(str, "(*t_ptr_)");
                    }
                    else {
                        hlp::append(str, "t_ptr_->");
                    }
                    render_cpp_call(func, str, alias);
                    hlp::append(str, ";\n        }\n\n    ");
                }
            }
            hlp::append(str, R"(};

    interface__* iface = nullptr;

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
        )a");
            if (any(flag & GenFlag::not_accept_null)) {
                hlp::append(str, R"(if(!utils::helper::deref(t)){
            return;
        }
        )");
            }
            hlp::append(str, R"a(iface=new implements__<std::decay_t<T>>(std::forward<T>(t));
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
            hlp::append(str, R"(explicit operator bool() const {
        return iface != nullptr;
    }

    ~)");
            hlp::append(str, iface.first);
            hlp::append(str, R"(() {
        delete iface;
    }

)");
            for (auto& func : iface.second) {
                if (func.funcname == decltype_func) {
                    hlp::append(str, R"(    template<class T>
    const T* type_assert() const {
        if (!iface) {
            return nullptr;
        }
        )");
                    if (use_dycast) {
                        hlp::append(str, R"(if(auto ptr=dynamic_cast<implements__<T>*>(iface)){
            return std::addressof(ptr->t_holder_);
        }
        return nullptr;)");
                    }
                    else {
                        hlp::append(str, R"(if (iface->type__()!=)");
                        if (has_other_typeinfo) {
                            hlp::append(str, data.typeid_func);
                        }
                        else {
                            hlp::append(str, "typeid(T)");
                        }
                        hlp::append(str, R"() {
            return nullptr;
        }
        return reinterpret_cast<const T*>(iface->raw__());)");
                    }
                    hlp::append(str, R"(
    }
    
)");
                    hlp::append(str, R"(    template<class T>
    T* type_assert() {
        if (!iface) {
            return nullptr;
        }
        )");
                    if (use_dycast) {
                        hlp::append(str, R"(if(auto ptr=dynamic_cast<implements__<T>*>(iface)){
            return std::addressof(ptr->t_holder_);
        }
        return nullptr;)");
                    }
                    else {
                        hlp::append(str, R"(if (iface->type__()!=)");
                        if (has_other_typeinfo) {
                            hlp::append(str, data.typeid_func);
                        }
                        else {
                            hlp::append(str, "typeid(T)");
                        }
                        hlp::append(str, R"() {
            return nullptr;
        }
        return reinterpret_cast<T*>(const_cast<void*>(iface->raw__()));)");
                    }
                    hlp::append(str, R"(
    }
    
)");
                }
                else if (func.funcname == copy_func) {
                    hlp::append(str, "    ");
                    hlp::append(str, iface.first);
                    hlp::append(str, "(const ");
                    hlp::append(str, iface.first);
                    hlp::append(str, R"(& in) {
        if(in.iface){
            iface=in.iface->copy__();
        }
    }
    
    )");
                    hlp::append(str, iface.first);
                    hlp::append(str, "& operator=(const ");
                    hlp::append(str, iface.first);
                    hlp::append(str, R"(& in) {
        delete iface;
        iface=nullptr;
        if(in.iface){
            iface=in.iface->copy__();
        }
        return *this;
    }

)");
                }
                else {
                    hlp::append(str, "    ");
                    render_cpp_function(func, str, alias);
                    hlp::append(str, R"({
        return iface?iface->)");
                    if (func.funcname == call_func) {
                        hlp::append(str, "operator()");
                    }
                    render_cpp_call(func, str, alias);
                    hlp::append(str, ":");
                    render_cpp_default_value(func, str, false, alias);
                    hlp::append(str, R"(;
    }

)");
                }
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

    bool generate(FileData& data, utw::string& str, Language lang, GenFlag flag) {
        if (lang == Language::cpp) {
            return generate_cpp(data, str, flag);
        }
        return false;
    }
}  // namespace ifacegen
