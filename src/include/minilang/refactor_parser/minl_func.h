/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minl_func - minilang function
#pragma once
#include "minlpass.h"
#include "minl_block.h"

namespace utils {
    namespace minilang {
        namespace parser {
            std::shared_ptr<FuncNode> func_parse_impl(func_expr_mode mode, const auto start, auto&& p) {
                auto error = [&](auto... msg) {
                    p.errc.say(msg...);
                    p.errc.trace(start, p.seq);
                    p.err = true;
                    return nullptr;
                };
                auto read_params = [&](std::shared_ptr<FuncParamNode>& par, bool is_retrun, std::shared_ptr<FuncParamNode>* root) {
                    auto single = "";
                    auto expect = "";
                    if (is_retrun) {
                        single = func_return_param_str_;
                        expect = "function return parameter";
                    }
                    else {
                        single = func_param_str_;
                        expect = "function parameter";
                    }
                    p.err = false;
                    if (helper::space::consume_space(p.seq, true); p.seq.seek_if(")")) {
                        goto END_OF_PARAM;
                    }
                    while (true) {
                        std::string param_name;
                        bool no_param_name = false;
                        const auto param_start = seq.rptr;
                        const char* type_str = single;
                        auto param_end = seq.rptr;
                        if (!read_parameter(p.seq, param_end, param_name, no_param_name)) {
                            return error("expect ", expect, " name identifier but not");
                        }
                        auto tytmp = p.type(p);
                        if (!tytmp) {
                            return error("expect ", expect, " type but not");
                        }
                        auto tmp = std::make_shared<FuncParamNode>();
                        tmp->str = type_str;
                        tmp->pos = {param_start, seq.rptr};
                        if (!no_param_name) {
                            tmp->ident = make_ident_node(param_name, {param_start, param_end});
                        }
                        tmp->type = std::move(tmp);
                        if (par) {
                            par->next = tmp;
                        }
                        else {
                            if (root) {
                                *root = tmp;
                            }
                        }
                        par = tmp;
                        helper::space::consume_space(p.seq, true);
                        if (!p.seq.seek_if(",")) {
                            if (p.seq.seek_if(")")) {
                                break;
                            }
                            return error("expect ", expect, " , or ) but not");
                        }
                        helper::space::consume_space(p.seq, true);
                    }
                END_OF_PARAM:
                    return true;
                };
                helper::space::consume_space(p.seq, true);
                std::string name;
                const auto ident_start = seq.rptr;
                auto ident_end = seq.rptr;
                if (mode == fe_def || mode == fe_iface) {
                    if (!ident_default_read(name, p.seq)) {
                        seq.rptr = start;
                        return false;
                    }
                    ident_end = seq.rptr;
                    helper::space::consume_space(p.seq, true);
                }
                if (!seq.seek_if("(")) {
                    return error("expect function parameter begin ( but not");
                }
                auto fnroot = std::make_shared<FuncNode>();
                const char* tys_ = func_dec_str_;
                std::shared_ptr<FuncParamNode> par = fnroot;
                p.err = false;
                read_params(par, false, nullptr);
                if (p.err) {
                    return true;
                }
                helper::space::consume_space(p.seq, false);
                if (!(seq.eos() || helper::match_eol<false>(p.seq) || seq.match("{") || seq.match("}"))) {
                    std::shared_ptr<MinNode> tmp;
                    if (seq.seek_if("(")) {
                        std::shared_ptr<FuncParamNode> ret, root;
                        read_params(ret, true, &root);
                        if (p.err) {
                            return true;
                        }
                        tmp = std::move(root);
                    }
                    else {
                        tmp = p.type(p);
                        if (!tmp) {
                            return error("expect function return type but not");
                        }
                    }
                    fnroot->return_ = std::move(tmp);
                    helper::space::consume_space(p.seq, false);
                }
                const auto no_block = mode == fe_tydec || mode == fe_iface;
                if (!no_block && p.seq.match("{")) {
                    p.err = false;
                    auto stat_node = block(call_stat())(p);
                    if (!stat_node) {
                        return error("expect function statement block but not");
                    }
                    fnroot->block = std::move(stat_node);
                    tys_ = func_def_str_;
                }
                else {
                    if (mode == fe_expr) {
                        return error("expect function statement begin { but not");
                    }
                }
                if (mode == fe_def) {
                    fnroot->ident = make_ident_node(name, {ident_start, ident_end});
                }
                fnroot->str = tys_;
                fnroot->pos = {start, p.seq.rptr};
                fnroot->mode = mode;
                return fnroot;
            }

            constexpr auto func_(func_expr_mode mode) {
                return [=](auto& p) -> std::shared_ptr<FuncNode> {
                    MINL_FUNC_LOG("func")
                    MINL_BEGIN_AND_START(p.seq);
                    if (!expect_ident(p.seq, "fn")) {
                        p.seq.rptr = begin;
                        return false;
                    }
                    return func_parse_impl(mode, start, p);
                };
            }
        }  // namespace parser
    }      // namespace minilang
}  // namespace utils
