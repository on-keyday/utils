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
    namespace us = utils::syntax;

    bool read_callback(utils::syntax::MatchContext<utw::string, utw::vector>& result, State& state) {
        if (result.result.token == "package") {
            return true;
        }
        if (result.top() == package_def && result.result.kind == us::KeyWord::id) {
            state.data.pkgname = result.result.token;
        }
        if (state.prevtoken == "interface") {
            state.current_iface = result.result.token;
        }
        if (result.top() == func_def) {
            if (result.result.kind == us::KeyWord::id) {
                return state.data.ifaces.insert({result.result.token, {}}).second;
            }
        }
    }
}  // namespace ifacegen