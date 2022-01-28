/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "interface_list.h"

namespace ifacegen {
    constexpr auto package_def = "PACKAGE";
    constexpr auto interface_def = "INTERFACE";
    constexpr auto func_def = "FUNCDEF";
    constexpr auto param_def = "VARDEF";
    constexpr auto type_def = "TYPE";
    constexpr auto pointer_ = "POINTER";
    constexpr auto type_prim = "TYPEPRIM";
    constexpr auto alias_def = "ALIAS";
    constexpr auto macro_def = "MACRO";
    constexpr auto import_def = "IMPORT";
    constexpr auto typeparam_def = "TYPEPARAM";
    constexpr auto typename_def = "TYPENAME";
    constexpr auto deftype_def = "DEFTYPE";
    constexpr auto typeid_def = "TYPEID";
    namespace us = utils::syntax;

    bool read_callback(utils::syntax::MatchContext<utw::string, utw::vector>& result, State& state) {
        if (result.top() == package_def && result.result.kind == us::KeyWord::id) {
            state.data.pkgname = result.token();
            return true;
        }
        if (result.top() == interface_def) {
            if (result.result.kind == us::KeyWord::id) {
                state.current_iface = result.token();

                auto res = state.data.ifaces.insert({state.current_iface, {}});
                if (!res.second) {
                    return false;
                }
                state.data.defvec.push_back(state.current_iface);
                res.first->second.typeparam = std::move(state.types);
                return true;
            }
            if (result.result.kind == us::KeyWord::eos) {
                state.current_iface = "";
                return true;
            }
        }
        if (result.top() == func_def) {
            if (result.result.kind == us::KeyWord::eos) {
                if (state.iface.type.ref != RefKind::none) {
                    state.data.has_ref_ret = true;
                }
                state.data.ifaces[state.current_iface].iface.push_back(std::move(state.iface));
                state.iface = {};
                return true;
            }
            if (result.token() == "const") {
                state.iface.is_const = true;
                return true;
            }
            if (result.result.kind == us::KeyWord::id) {
                if (state.iface.funcname.size()) {
                    state.iface.default_result = result.token();
                    if (state.iface.default_result == "panic") {
                        state.data.has_ref_ret = true;
                    }
                }
                else {
                    state.iface.funcname = result.token();
                }
                return true;
            }
        }
        if (result.top() == param_def) {
            if (result.result.kind == us::KeyWord::id) {
                state.iface.args.push_back({});
                state.iface.args.back().name = result.token();
            }
        }
        auto set_type = [&](auto& type) {
            if (result.token() == "const") {
                if (type.is_const) {
                    return false;
                }
                type.is_const = true;
            }
            else if (result.token() == "*") {
                type.pointer++;
            }
            else if (result.token() == "&&") {
                type.ref = RefKind::rval;
            }
            else if (result.token() == "&") {
                type.ref = RefKind::lval;
            }
            else if (result.token() == "...") {
                type.vararg = true;
            }
            else {
                type.prim = result.token();
            }
            return true;
        };
        if (result.top() == type_def || result.top() == pointer_ || result.top() == type_prim) {
            if (result.under(param_def)) {
                return set_type(state.iface.args.back().type);
            }
            else {
                return set_type(state.iface.type);
            }
        }
        if (result.top() == macro_def || result.top() == alias_def) {
            if (result.result.kind == us::KeyWord::id) {
                state.current_alias = result.token();
                if (!state.data.aliases.insert({state.current_alias, {.is_macro = result.top() == macro_def}}).second) {
                    return false;
                }
                if (!state.data.aliases.at(state.current_alias).is_macro) {
                    state.data.aliases.at(state.current_alias).types = std::move(state.types);
                    if (state.data.ifaces.find(state.current_alias) != state.data.ifaces.end()) {
                        return false;
                    }
                    state.data.defvec.push_back(state.current_alias);
                }
            }
            if (result.result.kind == us::KeyWord::until_eol) {
                state.data.aliases[state.current_alias].token = result.token();
                state.current_alias.clear();
            }
        }
        if (result.top() == import_def) {
            if (result.result.kind == us::KeyWord::until_eol) {
                state.data.headernames.push_back(result.token());
            }
        }
        if (result.top() == typeparam_def || result.top() == typename_def || result.top() == typeid_def) {
            if (result.token() == "...") {
                state.vararg = true;
            }
            else if (result.token() == "^") {
                state.template_param = true;
            }
            else if (result.result.kind == us::KeyWord::id) {
                state.types.push_back({.vararg = state.vararg, .template_param = state.template_param, .name = result.token()});
                state.vararg = false;
                state.template_param = false;
            }
        }
        if (result.top() == deftype_def) {
            if (result.result.kind == us::KeyWord::id) {
                state.types.back().defvalue = result.token();
            }
        }
        return true;
    }
}  // namespace ifacegen
