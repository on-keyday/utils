/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "type_list.h"
#include "../../include/syntax/dispatcher/filter.h"

namespace binred {
    namespace us = utils::syntax;
    constexpr auto package_def = "PACKAGE";
    constexpr auto struct_def = "STRUCT";
    constexpr auto member_def = "MEMBER";
    constexpr auto type_def = "TYPE";
    constexpr auto flag_def = "FLAG_DETAIL";
    constexpr auto import_def = "IMPORT";
    constexpr auto size_def = "SIZE";
    constexpr auto base_def = "BASE";
    constexpr auto bind_def = "BIND";
    constexpr auto prev_def = "PREV";
    constexpr auto expr_def = "EXPR";
    constexpr auto as_result_def = "AS_RESULT";
    bool read_fmt(utils::syntax::MatchContext<utw::string, utw::vector>& result, State& state) {
        constexpr auto is_expr = us::filter::stack_order(1, expr_def);
        if (is_expr(result)) {
            return state.tree(result, false) == us::MatchState::succeed;
        }
        if (result.top() == expr_def) {
            return true;
        }
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
        if (result.top() == as_result_def) {
            if (result.kind() == us::KeyWord::id ||
                result.kind() == us::KeyWord::integer) {
                auto set_to_flag = [&](utw::vector<Cond>& cond) {
                    cond.back().errvalue = result.token();
                };
                auto under_disp = [&](Type& type) {
                    if (result.under(prev_def)) {
                        set_to_flag(type.existcond);
                    }
                    /*else if (result.under(bind_def)) {
                        set_to_flag(type.aftercond);
                    }*/
                    else {
                        set_to_flag(type.prevcond);
                    }
                };
                if (result.under(base_def)) {
                    under_disp(cst.base.type);
                }
                else {
                    under_disp(memb().back().type);
                }
                return true;
            }
        }
        if (result.top() == flag_def) {
            auto set_to_flag = [&](auto& cond) {
                cond.push_back({std::move(state.tree.current)});
            };
            auto under_disp = [&](Type& type) {
                if (result.under(prev_def)) {
                    set_to_flag(type.existcond);
                }
                /*else if (result.under(bind_def)) {
                    set_to_flag(type.aftercond);
                }*/
                else {
                    set_to_flag(type.prevcond);
                }
            };
            if (result.under(base_def)) {
                under_disp(cst.base.type);
            }
            else {
                under_disp(memb().back().type);
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
            auto handle_size = [&](auto& size) {
                size = std::move(state.tree.current);
            };
            if (result.under(base_def)) {
                handle_size(cst.base.type.size);
            }
            else {
                auto& t = memb().back();
                handle_size(t.type.size);
            }
            return true;
        }
        return true;
    }
}  // namespace binred
