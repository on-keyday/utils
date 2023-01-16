/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// ident - identifier
#pragma once
#include "token.h"
#include "tokendef.h"

namespace utils {
    namespace minilang::token {

    

        constexpr auto ident_default() {
            return [](auto& seq) -> bool {
                return !seq.eos() &&
                       (!number::is_control_char(seq.current()) &&
                            !number::is_symbol_char(seq.current()) ||
                        seq.current() == '_');
            };
        }

        constexpr auto cond_read(auto&& cond) {
            return [=](auto& str, auto& seq) {
                while (cond(seq)) {
                    str.push_back(seq.current());
                    seq.consume();
                }
                return str.size() != 0;
            };
        }

        constexpr auto ident_default_read = cond_read(ident_default());

        template <class Read = decltype(ident_default_read)>
        constexpr auto yield_ident(Read&& ident = std::move(ident_default_read)) {
            return [=](auto&& src) -> std::shared_ptr<Ident> {
                const auto trace = trace_log(src, "ident");
                std::string tok;
                size_t b = src.seq.rptr;
                if (!ident(tok, src.seq)) {
                    return nullptr;
                }
                pass_log(src, "ident");
                auto id = std::make_shared_for_overwrite<Ident>();
                id->token = std::move(tok);
                id->pos.begin = b;
                id->pos.end = src.seq.rptr;
                id->first_lookup = lookup_first(src, id);
                return id;
            };
        };
    }  // namespace minilang::token
}  // namespace utils
