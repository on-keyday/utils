/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
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
    constexpr auto import_def = "IMPORT";
    namespace us = utils::syntax;

    bool read_callback(utils::syntax::MatchContext<utw::string, utw::vector>& result, State& state) {
        if (result.top() == package_def && result.result.kind == us::KeyWord::id) {
            state.data.pkgname = result.token();
            return true;
        }
        if (result.top() == interface_def) {
            if (result.result.kind == us::KeyWord::id) {
                state.current_iface = result.token();
                /*state.data.ifaceimpl.push_back({});
                auto idx = state.data.ifaceimpl.size() - 1;*/
                auto res = state.data.ifaces.insert({state.current_iface, {}});
                if (!res.second) {
                    return false;
                }
                state.data.defvec.push_back(state.current_iface);
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
                state.data.ifaces[state.current_iface].push_back(std::move(state.iface));
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
        if (result.top() == alias_def) {
            if (result.result.kind == us::KeyWord::id) {
                state.current_alias = result.token();
                if (!state.data.aliases.insert({state.current_alias, {}}).second) {
                    return false;
                }
            }
            if (result.result.kind == us::KeyWord::until_eol) {
                state.data.aliases[state.current_alias] = result.token();
                state.current_alias.clear();
            }
        }
        if (result.top() == import_def) {
            if (result.result.kind == us::KeyWord::until_eol) {
                state.data.headernames.push_back(result.token());
            }
        }
        return true;
    }
}  // namespace ifacegen
