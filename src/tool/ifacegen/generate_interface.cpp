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
    constexpr auto vtable_func = "__vtable__";
    constexpr auto sso_func = "__sso__";

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

    enum Vtable {
        vnone = 0x0,
        vfuncptr = 0x1,
        vfuncsignature = 0x2,
        vfunctail = 0x4,
    };

    void render_cpp_function(Interface& func, utw::string& str, utw::map<utw::string, Alias>* alias, bool on_iface, Vtable is_vtptr = vnone) {
        render_cpp_type(func.type, str, alias, on_iface);
        if (func.funcname == call_func) {
            if (is_vtptr) {
                return;
            }
            hlp::append(str, "operator()");
        }
        else if (func.funcname == array_op) {
            if (is_vtptr) {
                return;
            }
            hlp::append(str, "operator[]");
        }
        else {
            if (is_vtptr & vfuncptr) {
                hlp::append(str, "(*");
            }
            hlp::append(str, func.funcname);
            if (is_vtptr & vfuncptr) {
                hlp::append(str, ")");
            }
        }
        hlp::append(str, "(");
        bool is_first = true;
        auto make_this = [&] {
            if (func.is_const) {
                hlp::append(str, "const ");
            }
            hlp::append(str, "void* this__");
            is_first = false;
        };
        if (is_vtptr && !(is_vtptr & vfunctail)) {
            make_this();
        }
        for (auto& arg : func.args) {
            if (!is_first) {
                hlp::append(str, ", ");
            }
            render_cpp_type(arg.type, str, alias, on_iface);
            hlp::append(str, arg.name);
            is_first = false;
        }
        if (is_vtptr && is_vtptr & vfunctail) {
            if (!is_first) {
                hlp::append(str, ", ");
            }
            make_this();
        }
        hlp::append(str, ") ");
        if (!is_vtptr && func.is_const) {
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

    void render_cpp_call(Interface& func, utw::string& str, utw::map<utw::string, Alias>* alias, bool on_iface, Vtable is_vtptr = vnone) {
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
        if (is_vtptr && !(is_vtptr & vfunctail)) {
            hlp::append(str, "this__");
            is_first = false;
        }
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
        if (is_vtptr && (is_vtptr & vfunctail)) {
            if (!is_first) {
                hlp::append(str, ",");
            }
            hlp::append(str, "this__");
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
            if (!unsafe_raw && use_dycast) {
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
            if (!unsafe_raw && use_dycast) {
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

    bool is_special_name(auto& funcname) {
        return funcname == copy_func || funcname == array_op ||
               funcname == array_op || funcname == unsafe_func ||
               funcname == decltype_func || funcname == typeid_func ||
               funcname == vtable_func || funcname == sso_func;
    }

    bool render_cpp_vtable__class(utw::string& str, GenFlag flag, auto& iface, auto& alias, auto& nmspc) {
        bool no_vtable = true;
        Vtable tail = vnone;
        for (Interface& func : iface.second.iface) {
            if (func.funcname == vtable_func) {
                if (func.type.prim == "lastthis") {
                    tail = vfunctail;
                    break;
                }
            }
        }
        for (Interface& func : iface.second.iface) {
            if (is_special_name(func.funcname)) {
                continue;
            }
            if (no_vtable) {
                hlp::append(str, "\n   private:\n");
                hlp::append(str, "    struct vtable__t {\n");
                no_vtable = false;
            }
            hlp::append(str, "        ");
            render_cpp_function(func, str, alias, true, Vtable(vfuncptr | tail));
            hlp::append(str, "= nullptr;\n");
        }
        if (no_vtable) {
            iface.second.has_vtable = false;
            return false;
        }
        hlp::append(str, "    };\n\n");
        hlp::appends(str,
                     "    template<class T__v>\n",
                     "    struct vtable__instance__ {\n",
                     "       private:\n",
                     "        constexpr vtable__instance__() = default;\n",
                     //"     public:",
                     "        using this_type = std::remove_pointer_t<decltype(", nmspc, "deref(std::declval<std::decay_t<T__v>&>()))>;\n\n");
        for (Interface& func : iface.second.iface) {
            if (is_special_name(func.funcname)) {
                continue;
            }
            hlp::append(str, "        static ");
            render_cpp_function(func, str, alias, true, Vtable(vfuncsignature | tail));
            hlp::appends(str, "{\n",
                         "            ", "return static_cast<",
                         func.is_const ? "const " : "", "this_type*>(this__)->");
            render_cpp_call(func, str, alias, true);
            hlp::appends(str, ";\n",
                         "        }\n\n");
        }

        hlp::appends(str,
                     "       public:\n",
                     "        static const vtable__t* instantiate() noexcept {\n",
                     "            static vtable__t instance{\n");
        for (Interface& func : iface.second.iface) {
            if (is_special_name(func.funcname)) {
                continue;
            }
            hlp::appends(str, "                ",
                         "&vtable__instance__::", func.funcname, ",\n");
        }
        hlp::appends(str, "            };\n");
        hlp::appends(str,
                     "            return &instance;\n",
                     "        }\n",
                     "    };\n\n");
        hlp::appends(str,
                     "   public:\n",
                     "    struct vtable__interface__ {\n",
                     "       private:\n",
                     "        void* this__ = nullptr;\n",
                     "        const vtable__t* vtable__ = nullptr;\n",
                     "       public:\n",
                     "        constexpr vtable__interface__() = default;\n\n",
                     "        explicit operator bool() const {\n",
                     "             return this__!=nullptr&&vtable__!=nullptr;\n",
                     "        }\n\n",
                     "        const vtable__t* to_c_style_vtable() const {\n",
                     "            return vtable__;\n",
                     "        }\n\n",
                     "        void* to_c_style_this() const {\n",
                     "            return this__;\n",
                     "        }\n\n",
                     "        template<class T__v>\n",
                     "        vtable__interface__(T__v& v__)\n",
                     "            :this__(", nmspc, "deref(v__)),vtable__(vtable__instance__<T__v>::instantiate()){}\n\n");
        for (Interface& func : iface.second.iface) {
            if (is_special_name(func.funcname)) {
                continue;
            }
            hlp::append(str, "        ");
            render_cpp_function(func, str, alias, false);
            hlp::append(str, "{\n");
            hlp::appends(str, "            ", "return vtable__->");
            render_cpp_call(func, str, alias, true, tail);
            hlp::appends(str, ";\n", "        }\n\n");
        }
        hlp::append(str, "    };\n\n");
        return true;
    }

    void render_cpp_interface__class(utw::string& str, GenFlag flag, auto& iface,
                                     auto& append_typeid, auto& append_typefn, auto& alias) {
        hlp::append(str, "    struct ");
        if (any(flag & GenFlag::no_vtable)) {
            hlp::append(str, "NOVTABLE__ ");
        }
        hlp::append(str, R"(interface__ {
)");
        bool raw_gened = false, type_gened = false;
        auto has_sso = any(flag & GenFlag::use_small_size_opt);
        for (Interface& func : iface.second.iface) {
            if (func.funcname == decltype_func) {
                if (!raw_gened) {
                    render_cpp_raw__func("        ", flag, str, 0, append_typeid, append_typefn);
                    raw_gened = true;
                }
                if (!any(flag & GenFlag::use_dyn_cast) && !type_gened && iface.second.has_unsafe) {
                    render_cpp_type__func("        ", str, 0, append_typeid, append_typefn);
                    type_gened = true;
                }
            }
            else if (func.funcname == unsafe_func) {
                if (!raw_gened) {
                    render_cpp_raw__func("        ", flag, str, 0, append_typeid, append_typefn);
                    raw_gened = true;
                }
            }
            else if (func.funcname == copy_func) {
                hlp::appends(str,
                             "        virtual interface__* copy__(",
                             has_sso ? "void* __storage_box" : "",
                             ") const = 0;\n");
            }
            else if (func.funcname == typeid_func) {
                if (!type_gened) {
                    render_cpp_type__func("        ", str, 0, append_typeid, append_typefn);
                    type_gened = true;
                }
            }
            else if (func.funcname == vtable_func) {
                if (!iface.second.has_vtable) {
                    continue;
                }
                hlp::append(str, "        virtual vtable__interface__ vtable__get__() const noexcept = 0;\n");
            }
            else if (func.funcname == sso_func) {
                // ignore
            }
            else {
                hlp::append(str, "        virtual ");
                render_cpp_function(func, str, alias, true);
                hlp::append(str, "= 0;\n");
            }
        }
        if (has_sso) {
            hlp::append(str, "        virtual interface__* move__(void* __storage_box) = 0;\n");
        }
        hlp::append(str, R"(
        virtual ~interface__() = default;
    };
)");
    }

    void render_cpp_implements__class(std::string& str, GenFlag flag, auto& iface,
                                      auto& append_typeid, auto& append_typefn,
                                      auto& nmspc, auto& alias) {
        hlp::append(str,
                    R"(
    template<class T__>
    struct implements__ : interface__ {
        T__ t_holder_;

        template<class V__>
        implements__(V__&& args)
            :t_holder_(std::forward<V__>(args)){}

)");
        bool raw_gened = false, type_gened = false;
        bool has_sso = any(flag & GenFlag::use_small_size_opt);
        for (auto& func : iface.second.iface) {
            if (func.funcname == decltype_func) {
                if (!raw_gened) {
                    render_cpp_raw__func("        ", flag, str, 1, append_typeid, append_typefn);
                    raw_gened = true;
                }
                if (!any(flag & GenFlag::use_dyn_cast) && !type_gened && iface.second.has_unsafe) {
                    render_cpp_type__func("        ", str, 1, append_typeid, append_typefn);
                    type_gened = true;
                }
            }
            else if (func.funcname == unsafe_func) {
                if (!raw_gened) {
                    render_cpp_raw__func("        ", flag, str, 1, append_typeid, append_typefn);
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
                hlp::appends(str, "        interface__* copy__(",
                             has_sso ? "void* __storage_box" : "", ") const override {\n");
                if (has_sso) {
                    hlp::appends(str,
                                 "           using gen_type = implements__<T__>;\n",
                                 "           if constexpr (sizeof(gen_type) <= sizeof(void*)*", iface.second.sso_bufsize, ") {\n",
                                 "               return new(__storage_box) implements__<T__>(t_holder_);\n",
                                 "           }\n",
                                 "           else {\n",
                                 "               return new implements__<T__>(t_holder_);\n",
                                 "           }\n");
                }
                else {
                    hlp::append(str, "            return new implements__<T__>(t_holder_);\n");
                }
                hlp::append(str, "        }\n\n");
            }
            else if (func.funcname == vtable_func) {
                if (!iface.second.has_vtable) {
                    continue;
                }
                hlp::appends(str,
                             "        vtable__interface__ vtable__get__() const noexcept override {\n",
                             "            return vtable__interface__(const_cast<T__&>(t_holder_));\n",
                             "        }\n\n");
            }
            else if (func.funcname == sso_func) {
                // ignore
            }
            else {
                hlp::append(str, "        ");
                render_cpp_function(func, str, alias, true);
                hlp::appends(str,
                             "override {\n",
                             "            auto t_ptr_ = ", nmspc, "deref(this->t_holder_);\n",
                             "            ");
                if (func.is_noexcept) {
                    hlp::appends(str, "static_assert(noexcept(");
                    render_cpp_t_ptr_call(str, alias, func);
                    hlp::appends(str, R"(),"expect noexcept function call but not");)", "\n");
                    hlp::append(str, "            ");
                }
                hlp::appends(str,
                             "if (!t_ptr_) {\n",
                             "                ");
                render_cpp_default_value(func, str, true, alias);
                hlp::appends(str,
                             ";\n",
                             "            }\n",
                             "            return ");
                render_cpp_t_ptr_call(str, alias, func);
                hlp::appends(str,
                             ";\n",
                             "        }\n\n");
            }
        }
        if (has_sso) {
            hlp::appends(str,
                         "        interface__* move__(void* __storage_box) override {\n",
                         "           using gen_type = implements__<T__>;\n",
                         "           if constexpr (sizeof(gen_type) <= sizeof(void*)*", iface.second.sso_bufsize, ") {\n",
                         "               return new(__storage_box) implements__<T__>(std::move(t_holder_));\n",
                         "           }\n",
                         "           else {\n",
                         "               return nullptr;\n",
                         "           }\n",
                         "        }\n\n");
        }
        hlp::append(str, R"(    };
)");
    }

    void render_cpp_public_members(utw::string& str, GenFlag flag, auto& iface, auto& append_typeid, auto& append_typefn, auto& alias) {
        bool has_cpy = false;
        auto has_sso = any(flag & GenFlag::use_small_size_opt);
        for (auto& func : iface.second.iface) {
            if (func.funcname == decltype_func) {
                render_cpp_type_assert_func(true, str, flag, append_typefn);
                render_cpp_type_assert_func(false, str, flag, append_typefn);
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
                hlp::appends(str,
                             "    ", iface.first, "(const ", iface.first, "& in) {\n",
                             "        if(in.iface){\n",
                             "            iface=in.iface->copy__(",
                             has_sso ? "__storage_box" : "", ");\n",
                             "        }",
                             "    }\n\n");
                hlp::appends(str,
                             "    ", iface.first, "& operator=(const ", iface.first, "& in) {\n", "        if(std::addressof(in)==this)return *this;\n");
                if (has_sso) {
                    hlp::append(str, "        delete___();\n");
                }
                else {
                    hlp::appends(str,
                                 "        delete iface;\n",
                                 "        iface=nullptr;\n");
                }
                hlp::appends(str,
                             "        if(in.iface){\n",
                             "            iface=in.iface->copy__(", has_sso ? "__storage_box" : "", ");\n",
                             "        }\n",
                             "        return *this;\n",
                             "    }\n\n");
                hlp::appends(str,
                             "    ", iface.first, "(", iface.first, "& in) : ", iface.first, "(const_cast<const ", iface.first, "&>(in)) {}\n\n",
                             "    ", iface.first, "& operator=(", iface.first, "& in){\n",
                             "        ", "return ", "*this = ", "const_cast<const ", iface.first, "&>(in);\n",
                             "    }\n\n");
            }
            else if (func.funcname == vtable_func) {
                if (!iface.second.has_vtable) {
                    continue;
                }
                hlp::appends(str,
                             "    vtable__interface__ get_self_vtable() const noexcept {\n",
                             "         return iface?iface->vtable__get__():vtable__interface__{};\n",
                             "    }\n\n");
                hlp::appends(str,
                             "    template<class T__v>\n",
                             "    static vtable__interface__ get_vtable(T__v& v) noexcept {\n",
                             "         return vtable__interface__(v);\n",
                             "    }\n\n");
            }
            else if (func.funcname == sso_func) {
                // ignore
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
    }

    void render_cpp_common_members(utw::string& str, GenFlag flag, utw::string& nmspc, auto& iface) {
        auto has_sso = any(flag & GenFlag::use_small_size_opt);
        if (has_sso) {
            hlp::appends(str,
                         "\n",
                         "    union {\n",
                         "        char __storage_box[sizeof(void*)*(1+(", iface.second.sso_bufsize, "))]{0};\n",
                         "        std::max_align_t __align_of;\n",
                         "        struct {\n",
                         "            interface__* __place_holder[", iface.second.sso_bufsize, "];\n",
                         "            interface__* iface;\n",
                         "        };\n",
                         "    };\n\n",
                         "    template<class T__>\n",
                         "    void new___(T__&& v) {\n",
                         "        interface__* p = nullptr;\n",
                         "        using gen_type= implements__<std::decay_t<T__>>;\n",
                         "        if constexpr (sizeof(gen_type)<=sizeof(void*)*", iface.second.sso_bufsize, ") {\n",
                         "            p = new (__storage_box) gen_type(std::forward<T__>(v));\n",
                         "        }\n",
                         "        else {\n",
                         "            p = new gen_type(std::forward<T__>(v));\n",
                         "        }\n",
                         "        iface = p;\n",
                         "    }\n\n",
                         "    bool is_local___() const {\n",
                         "        return static_cast<const void*>(__storage_box)==static_cast<const void*>(iface);\n",
                         "    }\n\n",
                         "    void delete___() {\n",
                         "        if(!iface)return;\n"
                         "        if(!is_local___()) {\n",
                         "            delete iface;\n",
                         "        }\n",
                         "        else {\n",
                         "            iface->~interface__();\n",
                         "        }\n",
                         "        iface=nullptr;\n",
                         "    }\n\n");
        }
        else {
            hlp::append(str, R"(
    interface__* iface = nullptr;

)");
        }
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
        if (has_sso) {
            hlp::append(str, "new___(std::forward<T__>(t));");
        }
        else {
            hlp::append(str, "iface = new implements__<std::decay_t<T__>>(std::forward<T__>(t));");
        }
        hlp::append(str, R"a(
    }

)a");
        hlp::appends(str,
                     "    ", has_sso ? "" : "constexpr ",
                     iface.first, "(", iface.first, "&& in) ",
                     "noexcept ",
                     "{\n");
        auto write_moveimpl = [&] {
            if (has_sso) {
                hlp::appends(str,
                             "        // reference implementation: MSVC std::function\n",
                             "        if (in.is_local___()) {\n",
                             "            iface = in.iface->move__(__storage_box);\n",
                             "            in.delete___();\n",
                             "        }\n",
                             "        else {\n"
                             "            iface = in.iface;\n",
                             "            in.iface = nullptr;\n",
                             "        }\n");
            }
            else {
                hlp::appends(str,
                             "        iface=in.iface;\n",
                             "        in.iface=nullptr;\n");
            }
        };
        write_moveimpl();
        hlp::append(str,
                    "    }\n\n");
        hlp::appends(str,
                     "    ", iface.first, "& operator=(", iface.first, "&& in) noexcept {\n",
                     "        if(this==std::addressof(in))return *this;\n");
        hlp::appends(str, "        ",
                     has_sso ? "delete___();\n" : "delete iface;\n");
        write_moveimpl();
        hlp::appends(str,
                     "        return *this;\n",
                     "    }\n\n");
        hlp::append(str, R"(    explicit operator bool() const noexcept {
        return iface != nullptr;
    }

)");
        hlp::appends(str, R"(    bool operator==(std::nullptr_t) const noexcept {
        return iface == nullptr;
    }
    
)");
        hlp::appends(str,
                     "    ~", iface.first, "() {\n");
        hlp::appends(str, "        ",
                     has_sso ? "delete___();" : "delete iface;\n");
        hlp::append(str, "    }\n\n");
    }

    void render_cpp_single_struct(utw::string& str, GenFlag flag, utw::string& nmspc, auto& iface,
                                  auto& alias, auto& append_typeid, auto& append_typefn) {
        if (iface.second.typeparam.size()) {
            render_cpp_template(str, iface.second.typeparam);
        }
        hlp::appends(str, "struct ", iface.first, " {\n");
        if (iface.second.has_vtable) {
            render_cpp_vtable__class(str, flag, iface, alias, nmspc);
        }

        hlp::append(str,
                    R"(
   private:

)");

        render_cpp_interface__class(str, flag, iface, append_typeid, append_typefn, alias);
        render_cpp_implements__class(str, flag, iface, append_typeid, append_typefn, nmspc, alias);
        render_cpp_common_members(str, flag, nmspc, iface);
        render_cpp_public_members(str, flag, iface, append_typeid, append_typefn, alias);
        hlp::append(str, "};\n\n");
    }

    bool generate_cpp(FileData& data, utw::string& str, GenFlag flag) {
        if (any(flag & GenFlag::add_license)) {
            hlp::append(str, "/*license*/\n");
        }
        hlp::append(str, R"(// Code generated by ifacegen (https://github.com/on-keyday/utils)

#pragma once
)");
        utw::string nmspc;
        if (data.has_sso_align) {
            hlp::append(str, "#include<cstddef>\n");
        }
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
            auto f = flag;
            if (iface.second.has_unsafe) {
                f |= GenFlag::unsafe_raw;
            }
            if (iface.second.has_sso) {
                f |= GenFlag::use_small_size_opt;
                if (!iface.second.sso_bufsize.size()) {
                    iface.second.sso_bufsize = "7";
                }
            }
            render_cpp_single_struct(str, f, nmspc, iface, alias, append_typeid, append_typefn);
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
