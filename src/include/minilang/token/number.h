/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// number - number token
#pragma once

#include "token.h"

#include "../../number/parse.h"
#include "../../number/prefix.h"
#include "tokendef.h"

namespace utils {
    namespace minilang::token {

        template <class TypeConfigs = number::internal::ReadConfig>
        constexpr auto yield_number(int radix, bool allow_float, TypeConfigs&& conf = number::internal::ReadConfig{}) {
            return [=](auto&& src) -> std::shared_ptr<Number> {
                auto trace = trace_log(src, "number");
                std::string tok;
                bool is_float = false;
                bool* cond = allow_float ? &is_float : nullptr;
                size_t b = src.seq.rptr;
                number::NumErr err = number::read_number(tok, src.seq, radix, cond, conf);
                if (err.e != number::NumError::none) {
                    return nullptr;
                }
                pass_log(src, "number");
                auto num = std::make_shared<Number>();
                num->token = std::move(tok);
                num->num_str = num->token;
                num->radix = radix;
                num->is_float = is_float;
                num->pos.begin = b;
                num->pos.end = src.seq.rptr;
                num->is_float = is_float;
                num->int_err = number::parse_integer(num->token, num->integer, radix, conf);
                num->float_err = number::parse_float(num->token, num->float_, radix, conf);
                return num;
            };
        }

        template <class TypeConfigs = number::internal::ReadConfig>
        constexpr auto yield_radix_number(bool allow_float, TypeConfigs&& conf = number::internal::ReadConfig{}) {
            return [=](auto& src) -> std::shared_ptr<Number> {
                auto radix = number::has_prefix(src.seq);
                auto b = src.seq.rptr;
                if (radix) {
                    maybe_log(src, "number");
                    char d[3] = {
                        '0',
                        src.seq.current(1),
                    };
                    src.seq.consume(2);
                    auto res = yield_number(radix, allow_float, conf)(src);
                    if (!res) {
                        src.seq.rptr = b;
                        return nullptr;
                    }
                    res->pos.begin = b;
                    res->token = d;
                    res->token.append(res->num_str);
                    return res;
                }
                return yield_number(10, allow_float, conf)(src);
            };
        }

    }  // namespace minilang::token
}  // namespace utils
