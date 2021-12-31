/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "type_list.h"

namespace binred {
    constexpr auto struct_def = "STRUCT";
    constexpr auto member_def = "MEMBER";
    constexpr auto flag_def = "FLAG";
    int read_fmt(utils::syntax::MatchContext<utw::string, utw::vector>& result, State& state) {
        if (result.top() == struct_def) {
            if (result.kind()) {
            }
        }
    }
}  // namespace binred
