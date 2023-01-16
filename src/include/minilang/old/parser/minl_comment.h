/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minl_comment - comment
#pragma once
#include "minlpass.h"

namespace utils {
    namespace minilang {
        namespace parser {

            struct ComBr {
                const char* begin = nullptr;
                const char* end = nullptr;
                bool nest = false;
            };

            constexpr auto comment(auto... com) {
                return [=](auto&& p) -> std::shared_ptr<CommentNode> {
                    MINL_FUNC_LOG("comment")
                    auto line_comment = [&](auto& comment) {
                        while (!p.seq.eos() && !helper::match_eol<true>(p.seq)) {
                            comment.push_back(p.seq.current());
                            p.seq.consume();
                        }
                        comment.push_back('\n');
                    };
                    auto block_comment = [&](const auto start, auto com, auto& comment) {
                        size_t count = 1;
                        while (true) {
                            if (p.seq.eos()) {
                                p.errc.say("expect end of comment ", com.end, " but reached eof");
                                p.errc.trace(start, p.seq);
                                p.err = true;
                                return false;
                            }
                            if (p.seq.seek_if(com.end)) {
                                helper::append(comment, com.end);
                                count--;
                                if (count == 0) {
                                    break;
                                }
                                continue;
                            }
                            if (com.nest && p.seq.seek_if(com.begin)) {
                                count++;
                                helper::append(comment, com.begin);
                                continue;
                            }
                            comment.push_back(p.seq.current());
                            p.seq.consume();
                        }
                        return true;
                    };
                    std::shared_ptr<CommentNode> root, comnode;
                    const auto start = p.seq.rptr;
                    auto fold = [&](auto com) {
                        if (p.err) {
                            return false;
                        }
                        MINL_BEGIN_AND_START(p.seq);
                        if (p.seq.seek_if(com.begin)) {
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
                            cnode->pos = {start, p.seq.rptr};
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
                    if (p.err) {
                        return nullptr;
                    }
                    if (!root) {
                        p.seq.rptr = start;
                        return nullptr;
                    }
                    return root;
                };
            }
        }  // namespace parser
    }      // namespace minilang

}  // namespace utils
