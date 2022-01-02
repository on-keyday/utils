/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "type_list.h"

namespace binred {
    namespace us = utils::syntax;
    constexpr auto package_def = "PACKAGE";
    constexpr auto struct_def = "STRUCT";
    constexpr auto member_def = "MEMBER";
    constexpr auto type_def = "TYPE";
    constexpr auto flag_def = "FLAG";
    constexpr auto import_def = "IMPORT";
    constexpr auto size_def = "SIZE";
    constexpr auto base_def = "BASE";
    bool read_fmt(utils::syntax::MatchContext<utw::string, utw::vector>& result, State& state) {
        if (result.top() == import_def) {
            if (result.kind() == us::KeyWord::until_eol) {
                state.data.imports.push_back(result.token());
            }
            return true;
        }
        if (result.top() == package_def) {
            if (result.kind() == us::KeyWord::id) {
                state.data.pkgname = result.token();
            }
            return true;
        }
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
        auto is_rval = [&] {
            return result.kind() == us::KeyWord::id || result.kind() == us::KeyWord::string || result.kind() == us::KeyWord::integer;
        };
        if (result.top() == member_def) {
            if (result.kind() == us::KeyWord::id) {
                memb().push_back({.name = result.token()});
            }
            else if (is_rval()) {
                auto& t = memb().back();
                t.defval = result.token();
                t.kind = result.kind();
            }
            return true;
        }
        auto& t = memb().back();
        if (result.top() == type_def) {
            if (!t.type.name.size() && result.kind() == us::KeyWord::id) {
                t.type.name = result.token();
            }
            else if (is_rval()) {
            }
            return true;
        }
        if (result.top() == flag_def) {
            auto set_to_flag = [&](auto& flag) {
                if (!flag.depend.size() && result.kind() == us::KeyWord::id) {
                    flag.depend = result.token();
                }
                else if (is_rval()) {
                    flag.val = result.token();
                    flag.kind = result.kind();
                }
                else if (result.kind() == us::KeyWord::literal_keyword) {
                    auto& type = flag.type;
                    if (result.token() == "eq") {
                        type = FlagType::eq;
                    }
                    else if (result.token() == "bit") {
                        type = FlagType::bit;
                    }
                    else if (result.token() == "ls") {
                        type = FlagType::ls;
                    }
                    else if (result.token() == "gt") {
                        t.type.flag.type = FlagType::gt;
                    }
                    else if (result.token() == "egt") {
                        type = FlagType::egt;
                    }
                    else if (result.token() == "els") {
                        type = FlagType::els;
                    }
                    else if (result.token() == "nq") {
                        type = FlagType::nq;
                    }
                }
            };
            if (result.under(base_def)) {
                set_to_flag(state.data.structs[state.cuurent_struct].base.type.flag);
            }
            else {
                set_to_flag(t.type.flag);
            }
            return true;
        }
        if (result.top() == size_def) {
            if (is_rval()) {
                t.type.flag.size = result.token();
            }
            return true;
        }
        if (result.top() == base_def) {
            if (result.kind() == us::KeyWord::id) {
                state.data.structs[state.cuurent_struct].base.name = result.token();
            }
            return true;
        }
        return true;
    }
}  // namespace binred
