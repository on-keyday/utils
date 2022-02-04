/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "interface_list.h"
#include "../../include/helper/appender.h"
#include "../../include/helper/strutil.h"
#include "../../include/number/char_range.h"
#include <random>
#include <regex>

namespace ifacegen {
    namespace hlp = utils::helper;

    constexpr auto decltype_func = "decltype";
    constexpr auto copy_func = "__copy__";
    constexpr auto call_func = "__call__";
    constexpr auto array_op = "__array__";
    constexpr auto unsafe_func = "__unsafe__";
    constexpr auto typeid_func = "typeid";

    void resolve_alias(utw::string& str, utw::string& prim, utw::map<utw::string, Alias>* alias) {
        if (alias) {
            auto found = alias->find(prim);
            if (found != alias->end() && found->second.is_macro) {
                hlp::append(str, found->second.token);
            }
            else {
                hlp::append(str, prim);
            }
        }
        else {
            hlp::append(str, prim);
        }
    }

    void render_cpp_noref_type(Type& type, utw::string& str, utw::map<utw::string, Alias>* alias) {
        if (type.is_const) {
            hlp::append(str, "const ");
        }
        resolve_alias(str, type.prim, alias);
        for (size_t i = 0; i < type.pointer; i++) {
            hlp::append(str, "*");
        }
    }

    void render_cpp_type(Type& type, utw::string& str, utw::map<utw::string, Alias>* alias, bool on_iface) {
        render_cpp_noref_type(type, str, alias);
        if (on_iface && type.vararg) {
            hlp::append(str, "&&");
        }
        else if (type.ref == RefKind::lval) {
            hlp::append(str, "&");
        }
        else if (type.ref == RefKind::rval) {
            hlp::append(str, "&&");
        }
        if (type.vararg) {
            hlp::append(str, "...");
        }
        hlp::append(str, " ");
    }

    void render_cpp_function(Interface& func, utw::string& str, utw::map<utw::string, Alias>* alias, bool on_iface) {
        render_cpp_type(func.type, str, alias, on_iface);
        if (func.funcname == call_func) {
            hlp::append(str, "operator()");
        }
        else if (func.funcname == array_op) {
            hlp::append(str, "operator[]");
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
            render_cpp_type(arg.type, str, alias, on_iface);
            hlp::append(str, arg.name);
            is_first = false;
        }
        hlp::append(str, ") ");
        if (func.is_const) {
            hlp::append(str, "const ");
        }
        if (func.type.ref != RefKind::none ||
            func.default_result == "panic") {
            func.is_noexcept = false;
        }
        if (func.is_noexcept) {
            hlp::append(str, "noexcept ");
        }
    }

    void render_cpp_call(Interface& func, utw::string& str, utw::map<utw::string, Alias>* alias, bool on_iface) {
        if (func.funcname == call_func || func.funcname == array_op) {
            //hlp::append(str, "operator()");
        }
        else {
            hlp::append(str, func.funcname);
        }
        if (on_iface && func.funcname == array_op) {
            hlp::append(str, "[");
        }
        else {
            hlp::append(str, "(");
        }
        bool is_first = true;
        for (auto& arg : func.args) {
            if (!is_first) {
                hlp::append(str, ", ");
            }
            bool make_forward = arg.type.vararg || arg.type.ref == RefKind::rval;
            if (make_forward) {
                hlp::append(str, "std::forward<");
                render_cpp_noref_type(arg.type, str, alias);
                hlp::append(str, ">(");
            }
            hlp::append(str, arg.name);
            if (make_forward) {
                hlp::append(str, ")");
            }
            if (arg.type.vararg) {
                hlp::append(str, "...");
            }
            is_first = false;
        }
        if (on_iface && func.funcname == array_op) {
            hlp::append(str, "]");
        }
        else {
            hlp::append(str, ")");
        }
    }

    void render_cpp_default_value(Interface& func, utw::string& str, bool need_ret, utw::map<utw::string, Alias>* alias) {
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

    void render_cpp_template(utw::string& str, utw::vector<TypeName>& typeparam) {
        hlp::append(str, "template<");
        bool is_first = true;
        for (auto& type : typeparam) {
            if (!is_first) {
                hlp::append(str, ", ");
            }
            if (type.template_param) {
                hlp::append(str, "template<typename...>");
            }
            hlp::append(str, "typename");
            if (type.vararg) {
                hlp::append(str, "...");
            }
            str.push_back(' ');
            hlp::append(str, type.name);
            if (type.defvalue.size()) {
                hlp::append(str, " = ");
                hlp::append(str, type.defvalue);
            }
            is_first = false;
        }
        hlp::append(str, ">\n");
    }

    void render_cpp_using(utw::string& str, auto& alias) {
        if (alias.second.types.size()) {
            render_cpp_template(str, alias.second.types);
        }
        hlp::append(str, "using ");
        hlp::append(str, alias.first);
        hlp::append(str, " = ");
        hlp::append(str, alias.second.token);
        hlp::append(str, ";\n\n");
    }

    void render_cpp_namespace_begin(utw::string& str, auto&& name) {
        hlp::append(str, "namespace ");
        hlp::append(str, name);
        hlp::append(str, " {\n");
    }

    void render_cpp_namespace_end(utw::string& str, auto&& name) {
        hlp::appends(str, "} // namespace ", name, "\n");
    }

    void render_cpp_typeidcond(auto& str, auto& cmpbase, auto&& baseindent, auto& append_typefn) {
        hlp::appends(str, baseindent, "if (", cmpbase, "!=");
        append_typefn();
        hlp::appends(str, ") {\n", baseindent, "    return nullptr;\n");
        hlp::appends(str, baseindent, "}\n");
    }

    void render_cpp_type__func(utw::string baseindent,
                               auto& str, int place, auto& append_typeid, auto& append_typefn) {
        if (place == 0) {
            hlp::append(str, baseindent);
            hlp::append(str, "virtual ");
            append_typeid();
            hlp::append(str, " type__() const noexcept = 0;\n");
        }
        else if (place == 1) {
            hlp::append(str, baseindent);
            append_typeid();
            hlp::append(str, " type__() const noexcept override {\n");
            hlp::appends(str, baseindent, "    return ");
            append_typefn();
            hlp::append(str, ";\n");
            hlp::appends(str, baseindent, "}\n\n");
        }
    }

    void render_cpp_raw__func(utw::string baseindent,
                              GenFlag flag, auto& str, int place,
                              auto& append_typeid, auto& append_typefn) {
        auto use_dycast = any(flag & GenFlag::use_dyn_cast);
        auto unsafe_raw = any(flag & GenFlag::unsafe_raw);
        if (place == 0) {
            if (use_dycast) {
                return;
            }
            hlp::append(str, baseindent);
            hlp::append(str, "virtual const void* raw__(");
            if (!unsafe_raw) {
                append_typeid();
            }
            hlp::append(str, ") const noexcept = 0;\n");
        }
        else if (place == 1) {
            if (use_dycast) {
                return;
            }
            hlp::append(str, baseindent);
            hlp::append(str, "const void* raw__(");
            if (!unsafe_raw) {
                append_typeid();
                hlp::append(str, "info__");
            }
            hlp::append(str, ") const noexcept override {\n");
            if (!unsafe_raw) {
                render_cpp_typeidcond(str, "info__", baseindent + "    ", append_typefn);
            }
            hlp::append(str, baseindent);
            hlp::append(str, "    ");
            hlp::append(str, "return static_cast<const void*>(std::addressof(t_holder_));\n");
            hlp::append(str, baseindent);
            hlp::append(str, "}\n\n");
        }
    }

    void render_cpp_type_assert_func(bool is_const, auto& str, GenFlag flag, auto& append_typefn) {
        auto use_dycast = any(flag & GenFlag::use_dyn_cast);
        auto unsafe_raw = any(flag & GenFlag::unsafe_raw);
        hlp::appends(str,
                     "    template<class T__>\n    ",
                     is_const ? "const " : "",
                     "T__* type_assert()", is_const ? " const" : "", R"( {
        if (!iface) {
            return nullptr;
        }
)");
        if (use_dycast) {
            hlp::append(str, "        ");
            hlp::append(str, R"(if(auto ptr=dynamic_cast<implements__<T__>*>(iface)){
            return std::addressof(ptr->t_holder_);
        }
        return nullptr;)");
        }
        else {
            if (unsafe_raw) {
                render_cpp_typeidcond(str, "iface->type__()", "        ", append_typefn);
            }
            hlp::append(str, "        ");
            hlp::appends(str, "return static_cast",
                         is_const ? "<const T__*>(" : "<T__*>(const_cast<void*>(",
                         "iface->raw__(");
            if (!unsafe_raw) {
                append_typefn();
            }
            hlp::append(str, is_const ? "));" : ")));");
        }
        hlp::append(str, R"(
    }

)");
    }

    void render_cpp_t_ptr_call(auto& str, auto& alias, Interface& func) {
        if (func.funcname == call_func || func.funcname == array_op) {
            hlp::append(str, "(*t_ptr_)");
        }
        else {
            hlp::append(str, "t_ptr_->");
        }
        render_cpp_call(func, str, alias, true);
    }

    bool generate_cpp(FileData& data, utw::string& str, GenFlag flag) {
        if (any(flag & GenFlag::add_license)) {
            hlp::append(str, "/*license*/\n");
        }
        hlp::append(str, R"(// Code generated by ifacegen (https://github.com/on-keyday/utils)

#pragma once
)");
        utw::string nmspc;
        if (any(flag & GenFlag::not_depend_lib)) {
            hlp::append(str, "#include<memory>\n");
            hlp::append(str, "#include<type_traits>\n");
            std::random_device dev;
            std::uniform_int_distribution uni(0, 35);
            nmspc.append("indep_");
            bool upper = false;
            for (auto i = 0; i < 20; i++) {
                nmspc.push_back(utils::number::to_num_char(uni(dev), upper));
                upper = !upper;
            }

            hlp::appends(str, "namespace ", nmspc, R"( {
    namespace internal {
        struct has_deref_impl {
            template<class T>
            static std::true_type test(decltype(*std::declval<T&>(),(void)0)*);
            template<class T>
            static std::false_type test(...);
        };

        template<class T>
        struct has_deref:decltype(has_deref_impl::template test<T>(nullptr)){};

        template<class T,bool f=has_deref<T>::value>
        struct deref_impl{
            using result=decltype(std::addressof(*std::declval<T&>()));
            constexpr static result deref(T& v){
                if(!v){
                    return nullptr;
                }
                return std::addressof(*v);
            }
        };

        template<class T>
        struct deref_impl<T,false>{
            using result=decltype(std::addressof(std::declval<T&>()));
            constexpr static result deref(T& v){
                return std::addressof(v);
            }
        };
    }

    template<class T>
    auto deref(T& t){
        return internal::deref_impl<T>::deref(t);
    }

    template<class T>
    T* deref(T* t){
        return t;
    }

}

)");
            nmspc.append("::");
        }
        else {
            hlp::append(str, "#include");
            if (data.helper_deref.size()) {
                hlp::append(str, data.helper_deref);
            }
            else {
                hlp::append(str, "<helper/deref.h>");
            }
            hlp::append(str, "\n");
            nmspc = "utils::helper::";
        }
        auto has_other_typeinfo = data.typeid_func.size() && data.typeid_type.size();
        auto use_dycast = any(flag & GenFlag::use_dyn_cast);
        //auto typeid_fn = any(flag & GenFlag::need_typeidfun);
        auto append_typeid = [&] {
            if (has_other_typeinfo) {
                hlp::append(str, data.typeid_type);
            }
            else {
                hlp::append(str, "const std::type_info&");
            }
        };
        auto append_typefn = [&](bool repvoid = false) {
            if (has_other_typeinfo) {
                if (!repvoid) {
                    hlp::append(str, data.typeid_func);
                }
                else {
                    auto copy = data.typeid_func;
                    copy = std::regex_replace(copy, std::regex("T__"), "void");
                    hlp::append(str, copy);
                }
            }
            else {
                if (!repvoid) {
                    hlp::append(str, "typeid(T__)");
                }
                else {
                    hlp::append(str, "typeid(void)");
                }
            }
        };
        //auto has_alloc = any(flag & GenFlag::use_allocator);
        if (data.has_ref_ret) {
            hlp::append(str, "#include<functional>\n");
        }
        for (auto& h : data.headernames) {
            hlp::appends(str, "#include", h, "\n");
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
        utw::map<utw::string, Alias>* alias = nullptr;
        if (!any(flag & GenFlag::expand_alias)) {
            for (auto& alias : data.aliases) {
                if (alias.second.is_macro) {
                    render_cpp_using(str, alias);
                }
            }
        }
        else {
            alias = &data.aliases;
        }
        for (auto& def : data.defvec) {
            auto found = data.ifaces.find(def);
            if (found == data.ifaces.end()) {
                auto found = data.aliases.find(def);
                auto& alias = *found;
                render_cpp_using(str, alias);
                continue;
            }
            auto& iface = *found;
            if (iface.second.typeparam.size()) {
                render_cpp_template(str, iface.second.typeparam);
            }
            hlp::appends(str, "struct ", iface.first, " {", R"(
   private:

)");

            hlp::append(str, "    struct ");
            if (any(flag & GenFlag::no_vtable)) {
                hlp::append(str, "NOVTABLE__ ");
            }
            hlp::append(str, R"(interface__ {
)");
            bool raw_gened = false, type_gened = false;
            for (auto& func : iface.second.iface) {
                if (func.funcname == decltype_func) {
                    if (!raw_gened) {
                        auto f = flag;
                        if (iface.second.has_unsafe) {
                            f |= GenFlag::unsafe_raw;
                        }
                        render_cpp_raw__func("        ", f, str, 0, append_typeid, append_typefn);
                        raw_gened = true;
                    }
                    if (!type_gened && iface.second.has_unsafe) {
                        render_cpp_type__func("        ", str, 0, append_typeid, append_typefn);
                        type_gened = true;
                    }
                }
                else if (func.funcname == unsafe_func) {
                    if (!raw_gened) {
                        render_cpp_raw__func("        ", flag | GenFlag::unsafe_raw, str, 0, append_typeid, append_typefn);
                        raw_gened = true;
                    }
                }
                else if (func.funcname == copy_func) {
                    hlp::append(str, "        virtual interface__* copy__() const = 0;\n");
                }
                else if (func.funcname == typeid_func) {
                    if (!type_gened) {
                        render_cpp_type__func("        ", str, 0, append_typeid, append_typefn);
                        type_gened = true;
                    }
                }
                else {
                    hlp::append(str, "        virtual ");
                    render_cpp_function(func, str, alias, true);
                    hlp::append(str, "= 0;\n    ");
                }
            }

            hlp::append(str, R"(
        virtual ~interface__(){}
    };
    
    template<class T__>
    struct implements__ : interface__ {
        T__ t_holder_;

        template<class V__>
        implements__(V__&& args)
            :t_holder_(std::forward<V__>(args)){}

)");
            raw_gened = false;
            type_gened = false;
            for (auto& func : iface.second.iface) {
                if (func.funcname == decltype_func) {
                    if (!raw_gened) {
                        auto f = flag;
                        if (iface.second.has_unsafe) {
                            f |= GenFlag::unsafe_raw;
                        }
                        render_cpp_raw__func("        ", f, str, 1, append_typeid, append_typefn);
                        raw_gened = true;
                    }
                    if (!type_gened && iface.second.has_unsafe) {
                        render_cpp_type__func("        ", str, 1, append_typeid, append_typefn);
                        type_gened = true;
                    }
                }
                else if (func.funcname == unsafe_func) {
                    if (!raw_gened) {
                        render_cpp_raw__func("        ", flag | GenFlag::unsafe_raw, str, 1, append_typeid, append_typefn);
                        raw_gened = true;
                    }
                }
                else if (func.funcname == typeid_func) {
                    if (!type_gened) {
                        render_cpp_type__func("        ", str, 1, append_typeid, append_typefn);
                        type_gened = true;
                    }
                }
                else if (func.funcname == copy_func) {
                    hlp::appends(str, R"(        interface__* copy__() const override {
            )",
                                 "return new implements__<T__>(t_holder_);", R"(
        }

)");
                }
                else {
                    hlp::append(str, "        ");
                    render_cpp_function(func, str, alias, true);
                    hlp::append(str, "override {\n");
                    hlp::append(str, "            ");
                    hlp::append(str, "auto t_ptr_ = ");
                    hlp::append(str, nmspc);
                    hlp::append(str, "deref(this->t_holder_);\n");
                    hlp::append(str, "            ");
                    if (func.is_noexcept) {
                        hlp::appends(str, "static_assert(noexcept(");
                        render_cpp_t_ptr_call(str, alias, func);
                        hlp::appends(str, R"(),"expect noexcept function call but not");)", "\n");
                        hlp::append(str, "            ");
                    }
                    hlp::append(str, "if (!t_ptr_) {\n");
                    hlp::append(str, "                ");
                    render_cpp_default_value(func, str, true, alias);
                    hlp::append(str, ";\n");
                    hlp::append(str, "            }\n");
                    hlp::append(str, "            ");
                    hlp::append(str, "return ");
                    render_cpp_t_ptr_call(str, alias, func);
                    hlp::append(str, ";");
                    hlp::append(str, "\n        }\n\n");
                }
            }
            hlp::append(str, R"(    };

    interface__* iface = nullptr;

)");
            hlp::append(str, R"(   public:
    constexpr )");
            hlp::append(str, iface.first);
            hlp::append(str, "(){}\n\n    constexpr ");
            hlp::append(str, iface.first);
            hlp::append(str, "(std::nullptr_t){}\n");
            hlp::append(str, R"(
    template <class T__>
    )");
            hlp::append(str, iface.first);
            hlp::appends(str, R"a((T__&& t) {
        static_assert(!std::is_same<std::decay_t<T__>,)a",
                         iface.first, R"a(>::value,"can't accept same type");
        )a");
            if (any(flag & GenFlag::not_accept_null)) {
                hlp::append(str, R"(if(!)");
                hlp::append(str, nmspc);
                hlp::append(str, R"(deref(t)){
            return;
        }
        )");
            }
            hlp::append(str, "iface=");
            hlp::append(str, "new implements__<std::decay_t<T__>>(std::forward<T__>(t));");
            hlp::append(str, R"a(
    }

    )a");
            hlp::appends(str, "constexpr ", iface.first);
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
        if(this==std::addressof(in))return *this;
        delete iface;
        iface=in.iface;
        in.iface=nullptr;
        return *this;
    }

    )");
            hlp::append(str, R"(explicit operator bool() const {
        return iface != nullptr;
    }

    )");
            hlp::appends(str, R"(bool operator==(std::nullptr_t) const {
        return iface == nullptr;
    }
    
    )");
            hlp::appends(str, "~", iface.first);
            hlp::append(str, R"(() {
        delete iface;
    }

)");
            bool has_cpy = false;
            for (auto& func : iface.second.iface) {
                if (func.funcname == decltype_func) {
                    auto f = flag;
                    if (iface.second.has_unsafe) {
                        f |= GenFlag::unsafe_raw;
                    }
                    render_cpp_type_assert_func(true, str, f, append_typefn);
                    render_cpp_type_assert_func(false, str, f, append_typefn);
                }
                else if (func.funcname == typeid_func) {
                    hlp::append(str, "    ");
                    append_typeid();
                    hlp::append(str, " type_id() const {");
                    hlp::append(str, R"(
        if (!iface){
            return )");
                    append_typefn(true);
                    hlp::append(str, R"(;
        }
        return iface->type__();
    }

)");
                }
                else if (func.funcname == unsafe_func) {
                    hlp::append(str, R"(    const void* unsafe_cast() const {
        if(!iface){
            return nullptr;
        }
        return iface->raw__();
    }

    void* unsafe_cast() {
        if(!iface){
            return nullptr;
        }
        return const_cast<void*>(iface->raw__());
    }

)");
                }
                else if (func.funcname == copy_func) {
                    has_cpy = true;
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
                    hlp::appends(str, R"(& in) {
        if(std::addressof(in)==this)return *this;
        delete iface;
        iface=nullptr;
        if(in.iface){
            iface=in.iface->copy__();
        }
        return *this;
    }

    )",
                                 iface.first, "(", iface.first, "& in) : ", iface.first, "(const_cast<const ", iface.first, "&>(in)) {}\n\n",
                                 "    ", iface.first, "& operator=(", iface.first, "& in){\n",
                                 "        ", "return ", "*this = ", "const_cast<const ", iface.first, "&>(in);\n",
                                 "    }\n\n");
                }
                else {
                    hlp::append(str, "    ");
                    render_cpp_function(func, str, alias, false);
                    hlp::append(str, R"({
        return iface?iface->)");
                    if (func.funcname == call_func) {
                        hlp::append(str, "operator()");
                    }
                    else if (func.funcname == array_op) {
                        hlp::append(str, "operator[]");
                    }
                    render_cpp_call(func, str, alias, false);
                    hlp::append(str, ":");
                    render_cpp_default_value(func, str, false, alias);
                    hlp::append(str, R"(;
    }

)");
                }
            }
            if (!has_cpy) {
                hlp::appends(str, "    ", iface.first, "(const ", iface.first, "&) = delete;\n\n");
                hlp::appends(str, "    ", iface.first, "& operator=(const ", iface.first, "&) = delete;\n\n");
                hlp::appends(str, "    ", iface.first, "(", iface.first, "&) = delete;\n\n");
                hlp::appends(str, "    ", iface.first, "& operator=(", iface.first, "&) = delete;\n\n");
            }
            hlp::append(str, "};\n\n");
        }
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
        return true;
    }

    bool generate(FileData& data, utw::string& str, Language lang, GenFlag flag) {
        if (lang == Language::cpp) {
            return generate_cpp(data, str, flag);
        }
        return false;
    }
}  // namespace ifacegen
