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
            }
            if (result.result.kind == us::KeyWord::eos) {
                state.current_iface = "";
            }
        }
        if (result.top() == func_def) {
            if (result.result.kind == us::KeyWord::eos) {
                state.data.ifaces[state.current_iface].push_back(std::move(state.iface));
                state.iface = {};
                return true;
            }
            if (result.token() == "const") {
                state.iface.is_const = true;
                return true;
            }
            if (result.result.kind == us::KeyWord::id) {
                state.iface.funcname = result.token();
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
                type.is_const = true;
            }
            else if (result.token() == "*") {
                type.pointer++;
            }
            else {
                type.prim = result.token();
            }
        };
        if (result.top() == type_def || result.top() == pointer_) {
            if (result.under(param_def)) {
                set_type(state.iface.args.back().type);
            }
            else {
                set_type(state.iface.type);
            }
        }
        return true;
    }
}  // namespace ifacegen