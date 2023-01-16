/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minl_import - import
#pragma once
#include "minlpass.h"

namespace utils {
    namespace minilang {
        namespace parser {
            constexpr auto import_field(auto& parse_after, auto& start, auto& curnode, auto& p) {
                return [&] {
                    const auto begin = seq.rptr;
                    space::consume_space(p.seq, true);
                    const auto start_as = p.seq.rptr;
                    const auto end_as = p.seq.rptr;
                    std::string as;
                    if (ident_default_read(as, p.seq)) {
                        end_as = seq.rptr;
                        space::consume_space(p.seq, false);
                    }
                    if (auto c = seq.current(); c != '"' && c != '\'' && c != '`') {
                        p.errc.say("expect import statement string but not");
                        p.errc.trace(start, p.seq);
                        p.err = true;
                        return;
                    }
                    auto r = p.prim(p);
                    if (!r) {
                        p.errc.say("expect import statement but not");
                        p.errc.trace(start, p.seq);
                        p.err = true;
                        return;
                    }
                    auto tmp = std::make_shared<ImportNode>();
                    tmp->str = imports_str_;
                    tmp->from = std::move(r);
                    tmp->pos = {start, seq.rptr};
                    if (as.size()) {
                        auto asnode = std::make_shared<MinNode>();
                        asnode->pos = {start, end_as};
                        asnode->str = std::move(as);
                        tmp->as = std::move(asnode);
                    }
                    curnode->next = tmp;
                    curnode = tmp;
                };
            }

            constexpr auto imports(auto&& parse_after) {
                return [=](auto&& p) -> std::shared_ptr<ImportNode> {
                    MINL_FUNC_LOG("imports")
                    MINL_BEGIN_AND_START(p.seq);
                    if (!expect_ident(p.seq, "import")) {
                        p.seq.rptr = begin;
                        return false;
                    }
                    std::shared_ptr<ImportNode> root, curnode;
                    auto read_import = import_field(parse_after, start, curnode, p);
                    root = std::make_shared<ImportNode>();
                    root->str = import_group_str_;
                    curnode = root;
                    space::consume_space(p.seq, true);
                    p.err = false;
                    if (seq.seek_if("(")) {
                        while (true) {
                            space::consume_space(p.seq, true);
                            if (seq.seek_if(")")) {
                                break;
                            }
                            read_import();
                            if (p.err) {
                                return nullptr;
                            }
                            p.err = false;
                        }
                    }
                    else {
                        read_import();
                    }
                    if (p.err) {
                        return nullptr;
                    }
                    root->pos = {start, seq.rptr};
                    return root;
                };
            }
        }  // namespace parser
    }      // namespace minilang
}  // namespace utils
