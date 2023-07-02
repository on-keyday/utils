/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// string - string literal
#pragma once
#include "../../escape/read_string.h"
#include "token.h"
#include "../../strutil/append.h"
#include"tokendef.h"

namespace utils {
    namespace minilang::token {
    

        template <class Prefix = decltype(escape::default_prefix()), class Escape = decltype(escape::default_set())>
        constexpr auto yield_string(escape::ReadFlag flag = escape::ReadFlag::parser_mode, Prefix&& prefix = escape::default_prefix(), Escape&& esc = escape::default_set()) {
            return [=](auto&& src) -> std::shared_ptr<String> {
                auto trace = trace_log(src, "string");
                std::string tok;
                size_t b = src.seq.rptr;
                auto prfix = src.seq.current();
                if (prefix(prfix)) {
                    maybe_log(src, "string");
                }
                if (!escape::read_string(tok, src.seq, flag | escape::ReadFlag::raw_unescaped & ~escape::ReadFlag::need_prefix, prefix, esc)) {
                    src.seq.rptr = b;
                    return nullptr;
                }
                pass_log(src, "string");
                auto str = std::make_shared<String>();
                str->pos.begin = b;
                str->pos.end = src.seq.rptr;
                char pf[] = {prfix, 0};
                strutil::appends(str->token, pf, tok, pf);
                str->unesc_err = escape::unescape_str(tok, str->unescaped, esc);
                str->prefix = prfix;
                return str;
            };
        }

    }  // namespace minilang::token
}  // namespace utils
