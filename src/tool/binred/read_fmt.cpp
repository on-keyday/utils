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
    constexpr auto import_def = "IMPORT";
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
        auto& t = memb().back();
        auto is_rval = [&] {
            return result.kind() == us::KeyWord::id || result.kind() == us::KeyWord::string || result.kind() == us::KeyWord::integer;
        };
        if (result.top() == type_def) {
            if (!t.type.name.size() && result.kind() == us::KeyWord::id) {
                t.type.name = result.token();
            }
            else if (is_rval()) {
                t.defval = result.token();
                t.kind = result.kind();
            }
            return true;
        }
        if (result.top() == flag_def) {
            if (!t.type.flag.depend.size() && result.kind() == us::KeyWord::id) {
                t.type.flag.depend = result.token();
            }
            else if (is_rval()) {
                t.type.flag.val = result.token();
                t.type.flag.kind = result.kind();
            }
            else if (result.kind() == us::KeyWord::literal_keyword) {
                if (result.token() == "eq") {
                    t.type.flag.type = FlagType::eq;
                }
                else if (result.token() == "bit") {
                    t.type.flag.type = FlagType::bit;
                }
            }
            return true;
        }
        if (result.top() == import_def) {
            if (result.kind() == us::KeyWord::until_eol) {
                state.data.imports.push_back(result.token());
            }
            return true;
        }
        return true;
    }
}  // namespace binred
