/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "type_list.h"

namespace binred {
    namespace us = utils::syntax;
    constexpr auto struct_def = "STRUCT";
    constexpr auto member_def = "MEMBER";
    constexpr auto flag_def = "FLAG";
    bool read_fmt(utils::syntax::MatchContext<utw::string, utw::vector>& result, State& state) {
        if (result.top() == struct_def) {
            if (result.kind() == us::KeyWord::id) {
                state.cuurent_struct = result.token();
                auto res = state.data.structs.insert({state.cuurent_struct, {}});
                if (!res.second) {
                    return false;
                }
                state.data.defvec.push_back(state.cuurent_struct);
            }
            return true;
        }
        if (result.top() == member_def) {
            state.data.structs[state.cuurent_struct].member.push_back({.name = result.token()});
        }
        if (result.top() == flag_def) {
        }
    }
}  // namespace binred
