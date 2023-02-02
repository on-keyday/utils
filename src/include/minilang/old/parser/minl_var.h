/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minl_var - minilang variable
#pragma once
#include "minlpass.h"

namespace utils {
    namespace minilang {
        namespace parser {
            constexpr auto let_() {
                return [](auto&& p) -> std::shared_ptr<LetNode> {
                    MINL_FUNC_LOG("let_stat")
                    MINL_BEGIN_AND_START(p.seq);
                    const char* typ_str;
                    const char* grp_str;
                    const char* expect;
                    if (expect_ident(p.seq, "let")) {
                        typ_str = let_def_str_;
                        grp_str = let_def_group_str_;
                        expect = "variable";
                    }
                    else if (expect_ident(p.seq, "const")) {
                        typ_str = const_def_str_;
                        grp_str = const_def_group_str_;
                        expect = "constant value";
                    }
                    else {
                        p.seq.rptr = begin;
                        return nullptr;
                    }
                    space::consume_space(p.seq, true);
                    std::shared_ptr<LetNode> root, letnode;
                    auto read_param_type = [&] {
                        const auto param_start = p.seq.rptr;
                        auto param_end = param_start;
                        std::string str;
                        while (true) {
                            if (!ident_default_read(str, p.seq)) {
                                p.errc.say("expected ", expect, " name identifier but not");
                                p.errc.trace(start, p.seq);
                                p.err = true;
                                return;
                            }
                            param_end = p.seq.rptr;
                            space::consume_space(p.seq, false);
                            if (seq.seek_if(",")) {
                                space::consume_space(p.seq, true);
                                str.push_back(',');
                                continue;
                            }
                            break;
                        }
                        std::shared_ptr<MinNode> typ, init;
                        if (!p.seq.match("=") && !p.seq.eos() && !space::parse_eol<false>(p.seq)) {
                            typ = p.type(p);
                            if (!typ) {
                                p.errc.say("expect ", expect, " type but not");
                                p.errc.trace(start, p.seq);
                                p.err = true;
                                return;
                            }
                            space::consume_space(p.seq, false);
                        }
                        if (p.seq.seek_if("=")) {
                            init = p.expr(p);
                            if (!init) {
                                p.errc.say("expect ", expect, " initialization expression but not");
                                p.errc.trace(start, p.seq);
                                p.err = true;
                                return;
                            }
                        }
                        auto lnode = std::make_shared<LetNode>();
                        lnode->str = typ_str;
                        lnode->ident = make_ident_node(str, {param_start, param_end});
                        lnode->type = std::static_pointer_cast<TypeNode>(typ);
                        lnode->init = std::move(init);
                        letnode->next = lnode;
                        letnode = lnode;
                    };
                    p.err = false;
                    const auto group_start = p.seq.rptr;
                    root = std::make_shared<LetNode>();
                    root->str = grp_str;
                    letnode = root;
                    if (seq.seek_if("(")) {
                        while (true) {
                            space::consume_space(p.seq, true);
                            if (seq.seek_if(")")) {
                                break;
                            }
                            read_param_type();
                            if (p.err) {
                                return true;
                            }
                        }
                    }
                    else {
                        read_param_type();
                    }
                    if (p.err) {
                        return true;
                    }
                    root->pos = {start, p.seq.rptr};
                    return root;
                };
            }
        }  // namespace parser
    }      // namespace minilang
}  // namespace utils
