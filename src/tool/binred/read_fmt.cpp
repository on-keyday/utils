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
        auto& cst = state.data.structs[state.cuurent_struct];
        auto memb = [&]() -> auto& {
            return cst.member;
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
        if (result.top() == flag_def) {
            auto set_to_flag = [&](auto& flag) {
                if (!flag.depend.size() && result.kind() == us::KeyWord::id) {
                    flag.depend = result.token();
                }
                else if (is_rval()) {
                    flag.val.val = result.token();
                    flag.val.kind = result.kind();
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
                        type = FlagType::gt;
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
                set_to_flag(cst.base.type.flag);
            }
            else {
                set_to_flag(memb().back().type.flag);
            }
            return true;
        }
        if (result.top() == base_def) {
            return true;
        }
        if (result.top() == type_def) {
            if (result.kind() == us::KeyWord::id) {
                if (result.under(base_def)) {
                    cst.base.type.name = result.token();
                }
                else {
                    auto& t = memb().back();

                    t.type.name = result.token();
                }
            }
            return true;
        }
        if (result.top() == size_def) {
            auto handle_size = [&](Size& size) {
                if (result.token() == "+") {
                    size.op = Op::add;
                }
                else if (result.token() == "-") {
                    size.op = Op::sub;
                }
                else if (is_rval()) {
                    if (!size.size1.val.size()) {
                        size.size1.val = result.token();
                        size.size1.kind = result.kind();
                    }
                    else {
                        size.size2.val = result.token();
                        size.size2.kind = result.kind();
                    }
                }
            };
            if (result.under(base_def)) {
                handle_size(cst.base.type.flag.size);
            }
            else {
                auto& t = memb().back();
                handle_size(t.type.flag.size);
            }
            return true;
        }
        return true;
    }
}  // namespace binred
