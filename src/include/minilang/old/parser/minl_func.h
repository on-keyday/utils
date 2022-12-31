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
                    if (space::consume_space(p.seq, true); p.seq.seek_if(")")) {
                        goto END_OF_PARAM;
                    }
                    while (true) {
                        std::string param_name;
                        bool no_param_name = false;
                        const auto param_start = p.seq.rptr;
                        const char* type_str = single;
                        auto param_end = p.seq.rptr;
                        if (!read_parameter(p.seq, param_end, param_name, no_param_name)) {
                            return error("expect ", expect, " name identifier but not");
                        }
                        auto tytmp = p.type(p);
                        if (!tytmp) {
                            return error("expect ", expect, " type but not");
                        }
                        auto tmp = std::make_shared<FuncParamNode>();
                        tmp->str = type_str;
                        tmp->pos = {param_start, p.seq.rptr};
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
                        space::consume_space(p.seq, true);
                        if (!p.seq.seek_if(",")) {
                            if (p.seq.seek_if(")")) {
                                break;
                            }
                            return error("expect ", expect, " , or ) but not");
                        }
                        space::consume_space(p.seq, true);
                    }
                END_OF_PARAM:
                    return true;
                };
                space::consume_space(p.seq, true);
                std::string name;
                const auto ident_start = p.seq.rptr;
                auto ident_end = p.seq.rptr;
                if (mode == fe_def || mode == fe_iface) {
                    if (!ident_default_read(name, p.seq)) {
                        p.seq.rptr = start;
                        return nullptr;
                    }
                    ident_end = p.seq.rptr;
                    space::consume_space(p.seq, true);
                }
                if (!p.seq.seek_if("(")) {
                    return error("expect function parameter begin ( but not");
                }
                auto fnroot = std::make_shared<FuncNode>();
                const char* tys_ = func_dec_str_;
                std::shared_ptr<FuncParamNode> par = fnroot;
                p.err = false;
                read_params(par, false, nullptr);
                if (p.err) {
                    return nullptr;
                }
                space::consume_space(p.seq, false);
                if (!(p.seq.eos() || helper::match_eol<false>(p.seq) ||
                      p.seq.match("{") || p.seq.match("}"))) {
                    std::shared_ptr<MinNode> tmp;
                    if (p.seq.seek_if("(")) {
                        std::shared_ptr<FuncParamNode> ret, root;
                        read_params(ret, true, &root);
                        if (p.err) {
                            return nullptr;
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
                    space::consume_space(p.seq, false);
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
                        return nullptr;
                    }
                    return func_parse_impl(mode, start, p);
                };
            }

            constexpr auto interface_() {
                return [](auto&& p) -> std::shared_ptr<InterfaceNode> {
                    MINL_FUNC_LOG("interface")
                    MINL_BEGIN_AND_START(p.seq);
                    if (!expect_ident(p.seq, "interface")) {
                        p.seq.rptr = begin;
                        return nullptr;
                    }
                    space::consume_space(p.seq, true);
                    if (!p.seq.seek_if("{")) {
                        p.errc.say("expect interface begin { but not");
                        p.errc.trace(start, p.seq);
                        p.err = true;
                        return nullptr;
                    }
                    auto ret = std::make_shared<InterfaceNode>();
                    ret->str = interface_str_;
                    auto cur = ret;
                    while (true) {
                        space::consume_space(p.seq, true);
                        if (p.seq.eos()) {
                            p.errc.say("unexpected EOF at interface declaration");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return nullptr;
                        }
                        if (p.seq.seek_if("}")) {
                            break;
                        }
                        p.err = false;
                        auto tmp = func_parse_impl(fe_iface, start, p);
                        if (!tmp) {
                            p.errc.say("expect interface method declaration but not");
                            p.errc.trace(start, p.seq);
                            p.err = true;
                            return nullptr;
                        }
                        auto meth = std::make_shared<InterfaceNode>();
                        meth->str = interface_method_str_;
                        meth->method = std::move(tmp);
                        cur->next = meth;
                        cur = meth;
                    }
                    ret->pos = {start, p.seq.rptr};
                    return ret;
                };
            }
        }  // namespace parser
    }      // namespace minilang
}  // namespace utils
