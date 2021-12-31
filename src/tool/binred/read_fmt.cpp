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
    constexpr auto type_def = "TYPE";
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
        auto memb = [&]() -> auto& {
            return state.data.structs[state.cuurent_struct].member;
        };
        if (result.top() == member_def) {
            memb().push_back({.name = result.token()});
        }
        if (result.top() == type_def) {
            if (result.kind() == us::KeyWord::id) {
                memb().back().type.name = result.token();
            }
            if (result.kind() == us::KeyWord::string || result.kind() == us::KeyWord::integer) {
                auto& t = memb().back();
                t.defval = result.token();
                t.kind = result.kind();
            }
            return true;
        }
        if (result.top() == flag_def) {
        }
    }
}  // namespace binred
