/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minlprog - minilang program
#pragma once
#include "minl.h"

namespace utils {
    namespace minilang {

        struct ComBr {
            const char* begin = nullptr;
            const char* end = nullptr;
            bool nest = false;
        };

        constexpr auto comment(auto... com) {
            return [=](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) -> bool {
                MINL_FUNC_LOG_OLD("comment")
                auto line_comment = [&](auto& comment) {
                    while (!seq.eos() && !space::parse_eol<true>(seq)) {
                        comment.push_back(seq.current());
                        seq.consume();
                    }
                    comment.push_back('\n');
                };
                auto block_comment = [&](const auto start, auto com, auto& comment) {
                    size_t count = 1;
                    while (true) {
                        if (seq.eos()) {
                            errc.say("expect end of comment ", com.end, " but reached eof");
                            errc.trace(start, seq);
                            err = true;
                            return false;
                        }
                        if (seq.seek_if(com.end)) {
                            helper::append(comment, com.end);
                            count--;
                            if (count == 0) {
                                break;
                            }
                            continue;
                        }
                        if (com.nest && seq.seek_if(com.begin)) {
                            count++;
                            helper::append(comment, com.begin);
                            continue;
                        }
                        comment.push_back(seq.current());
                        seq.consume();
                    }
                    return true;
                };
                std::shared_ptr<CommentNode> root, comnode;
                const auto start = seq.rptr;
                auto fold = [&](auto com) {
                    if (err) {
                        return false;
                    }
                    space::consume_space(seq, true);
                    const auto start = seq.rptr;
                    if (seq.seek_if(com.begin)) {
                        std::string comment;
                        helper::append(comment, com.begin);
                        if (com.end) {
                            if (!block_comment(start, com, comment)) {
                                return false;
                            }
                        }
                        else {
                            line_comment(comment);
                        }
                        auto cnode = std::make_shared<CommentNode>();
                        cnode->str = comment_str_;
                        cnode->comment = std::move(comment);
                        cnode->pos = {start, seq.rptr};
                        if (comnode) {
                            comnode->next = cnode;
                        }
                        else {
                            root = cnode;
                        }
                        comnode = cnode;
                        return true;
                    }
                    return false;
                };
                while ((... || fold(com))) {
                }
                if (err) {
                    return true;
                }
                if (!root) {
                    seq.rptr = start;
                    return false;
                }
                node = std::move(root);
                return true;
            };
        }

        constexpr auto until_eof_or_not_matched(auto&& f) {
            return [=](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) {
                MINL_FUNC_LOG_OLD("until_eof_or_not_matched")
                std::shared_ptr<BlockNode> root, bnode;
                err = false;
                space::consume_space(seq, true);
                while (!seq.eos()) {
                    const auto start = seq.rptr;
                    if (!f(f, expr, seq, node, err, errc)) {
                        break;
                    }
                    if (err) {
                        return true;
                    }
                    auto tmp = std::make_shared<BlockNode>();
                    tmp->str = program_str_;
                    tmp->element = std::move(node);
                    tmp->pos = {start, seq.rptr};
                    if (bnode) {
                        bnode->next = tmp;
                    }
                    else {
                        root = tmp;
                    }
                    bnode = tmp;
                    space::consume_space(seq, true);
                }
                node = std::move(root);
                return node != nullptr;
            };
        }

        constexpr auto import_field(auto& parse_after, auto& start, auto& seq, auto& curnode, bool& err, auto& errc) {
            return [&] {
                const auto begin = seq.rptr;
                space::consume_space(seq, true);
                const auto start_as = seq.rptr;
                auto end_as = seq.rptr;
                std::string as;
                if (ident_default_read(as, seq)) {
                    end_as = seq.rptr;
                    space::consume_space(seq, false);
                }
                if (auto c = seq.current(); c != '"' && c != '\'' && c != '`') {
                    errc.say("expect import statement string but not");
                    errc.trace(start, seq);
                    err = true;
                    return;
                }
                auto r = parse_after(seq, errc);
                if (!r) {
                    errc.say("expect import statement but not");
                    errc.trace(start, seq);
                    err = true;
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
            return [=](auto&& stat, auto&& expr, auto& seq, std::shared_ptr<MinNode>& node, bool& err, auto& errc) -> bool {
                MINL_FUNC_LOG_OLD("imports")
                const auto start = seq.rptr;
                if (!expect_ident(seq, "import")) {
                    return false;
                }
                std::shared_ptr<ImportNode> root, curnode;
                auto read_import = import_field(parse_after, start, seq, curnode, err, errc);
                root = std::make_shared<ImportNode>();
                root->str = import_group_str_;
                curnode = root;
                space::consume_space(seq, true);
                err = false;
                if (seq.seek_if("(")) {
                    while (true) {
                        space::consume_space(seq, true);
                        if (seq.seek_if(")")) {
                            break;
                        }
                        read_import();
                        if (err) {
                            return true;
                        }
                    }
                }
                else {
                    read_import();
                }
                if (err) {
                    return true;
                }
                root->pos = {start, seq.rptr};
                node = std::move(root);
                return true;
            };
        }
    }  // namespace minilang
}  // namespace utils
