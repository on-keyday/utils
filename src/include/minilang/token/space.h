/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// space - space and line
#pragma once
#include "token.h"
#include "../../helper/space.h"
#include "../../helper/pushbacker.h"
#include "../../helper/readutil.h"
#include "tokendef.h"

namespace utils {
    namespace minilang::token {

        constexpr auto yield_space() {
            return [](auto&& src) -> std::shared_ptr<Space> {
                const auto trace = trace_log(src, "space");
                size_t b = src.seq.rptr;
                auto m = space::match_space<false>(src.seq, false);
                if (m == 0) {
                    return nullptr;
                }
                char16_t v[]{m, 0};
                char pb[5]{0};
                utf::convert_one(v, helper::CharVecPushbacker(pb, 5));
                std::string tok;
                size_t count = 0;
                while (src.seq.seek_if(pb)) {
                    tok.append(pb);
                    count++;
                }
                pass_log(src, "space");
                auto sp = std::make_shared_for_overwrite<Space>();
                sp->token = std::move(tok);
                sp->space = m;
                sp->pos.begin = b;
                sp->pos.end = src.seq.rptr;
                sp->count = count;
                return sp;
            };
        }

        constexpr bool has_eol(auto& seq, bool& lf, bool& cr, const char*& line) {
            auto len = helper::match_eol<false>(seq);
            if (len == 0) {
                return false;
            }
            if (len == 2) {
                line = "\r\n";
                lf = cr = true;
            }
            else {
                if (seq.current() == '\r') {
                    line = "\r";
                    cr = true;
                }
                else {
                    line = "\n";
                    lf = true;
                }
            }
            return true;
        }

        constexpr auto yield_line() {
            return [](auto&& src) -> std::shared_ptr<Line> {
                const auto trace = trace_log(src, "line");
                size_t b = src.seq.rptr;
                const char* line = nullptr;
                bool lf = false, cr = false;
                if (!has_eol(src.seq, lf, cr, line)) {
                    return nullptr;
                }
                size_t count = 0;
                std::string tok;
                while (src.seq.seek_if(line)) {
                    tok.append(line);
                    count++;
                }
                pass_log(src, "line");
                auto ln = std::make_shared_for_overwrite<Line>();
                ln->token = std::move(tok);
                ln->pos.begin = b;
                ln->pos.end = src.seq.rptr;
                ln->count = count;
                ln->cr = cr;
                ln->lf = lf;
                return ln;
            };
        }

        struct SkipConfig {
            bool space = true;
            bool line = true;
            bool comment = true;
        };

        constexpr auto skip_spaces(SkipConfig conf) {
            return [=](auto&& then) {
                return [=](auto&& src, auto&&... args) -> std::shared_ptr<Token> {
                    size_t b = src.seq.rptr;
                    auto first = src.next;
                    while (load_next(src)) {
                        std::shared_ptr<Token>& next = src.next;
                        if (conf.space && next->kind == Kind::space) {
                            src.next = nullptr;
                            continue;
                        }
                        if (conf.line && next->kind == Kind::line) {
                            src.next = nullptr;
                            continue;
                        }
                        if (conf.comment && next->kind == Kind::comment) {
                            src.next = nullptr;
                            continue;
                        }
                        break;
                    }
                    if (std::shared_ptr<Token> tok = then(src, args...)) {
                        return tok;
                    }
                    src.next = std::move(first);
                    src.seq.rptr = b;
                    return nullptr;
                };
            };
        }

    }  // namespace minilang::token
}  // namespace utils
