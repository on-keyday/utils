/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// comment
#pragma once
#include "token.h"
#include "space.h"
#include "../../strutil/append.h"
#include "tokendef.h"

namespace utils {
    namespace minilang::token {

        constexpr auto yield_line_comment(auto begin) {
            return [=](auto&& src) -> std::shared_ptr<Comment> {
                auto trace = trace_log(src, "line_comment");
                size_t b = src.seq.rptr;
                if (src.seq.seek_if(begin)) {
                    return nullptr;
                }
                const char* eol = "";
                bool x, y;
                std::string tok;
                while (!src.seq.eos() && !has_eol(src.seq, x, y, eol)) {
                    tok.push_back(src.seq.current());
                    src.seq.consume();
                }
                src.seq.seek_if(eol);
                pass_log(src, "line_comment");
                auto cm = std::make_shared<Comment>();
                cm->detail = std::move(tok);
                strutil::appends(cm->token, begin, cm->detail, eol);
                cm->pos.begin = b;
                cm->pos.end = src.seq.rptr;
                cm->begin = begin;
                cm->end = eol;
                return cm;
            };
        }

        constexpr auto yield_block_comment(auto begin, auto end, bool recursive = true) {
            return [=](auto&& src) -> std::shared_ptr<Comment> {
                const auto trace = trace_log(src, "block_comment");
                size_t b = src.seq.rptr;
                if (!src.seq.seek_if(begin)) {
                    return nullptr;
                }
                size_t count = 1;
                std::string tok;
                while (!src.seq.eos()) {
                    if (recursive && src.seq.seek_if(begin)) {
                        tok.append(begin);
                        count++;
                        continue;
                    }
                    if (src.seq.seek_if(end)) {
                        count--;
                        if (count == 0) {
                            pass_log(src, "block_comment");
                            auto cm = std::make_shared<Comment>();
                            cm->detail = std::move(tok);
                            strutil::appends(cm->token, begin, cm->detail, end);
                            cm->pos.begin = b;
                            cm->pos.end = src.seq.rptr;
                            cm->begin = begin;
                            cm->end = end;
                        }
                        tok.append(end);
                        continue;
                    }
                    tok.push_back(src.seq.current());
                    src.seq.consume();
                }
                error_log(src,"unexpected EOF at reading block comment. `", end, "` is expected");
                src.err = true;
                return nullptr;
            };
        }
    }  // namespace minilang::token
}  // namespace utils
