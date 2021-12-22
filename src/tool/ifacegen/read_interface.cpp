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
    namespace us = utils::syntax;

    bool read_callback(utils::syntax::MatchContext<utw::string, utw::vector>& result, State& state) {
        if (result.top() == package_def && result.result.kind == us::KeyWord::id) {
            state.data.pkgname = result.result.token;
            return true;
        }
        if (result.top() == interface_def && result.result.kind == us::KeyWord::id) {
            state.current_iface = result.result.token;
            auto res = state.data.ifaces.insert({state.current_iface, {}});
            if (!res.second) {
                return false;
            }
        }
        if (result.top() == func_def) {
            if (result.result.token == "const") {
                state.iface.is_const = true;
                return true;
            }
            if (result.result.kind == us::KeyWord::id) {
                state.iface.funcname = result.result.token;
                return true;
            }
        }
        if (result.top() == param_def) {
            if (result.result.kind == us::KeyWord::id) {
                state.iface.args.push_back({});
                state.iface.args.back().name = result.result.token;
            }
        }
        if (result.top() == type_def) {
            if (result.under(param_def)) {
            }
        }
    }
}  // namespace ifacegen