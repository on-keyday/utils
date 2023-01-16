/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// reader - token reader
#pragma once

#include "token.h"

namespace utils {
    namespace tokenize {
        template <class String>
        struct Reader {
            using token_t = wrap::shared_ptr<Token<String>>;
            token_t root;
            token_t current;

            constexpr Reader() {}
            constexpr Reader(token_t& v)
                : root(v), current(v) {}

           protected:
            static bool default_ignore(const token_t& tok) {
                return tok->is(TokenKind::root) ||
                       tok->is(TokenKind::space) ||
                       tok->is(TokenKind::line) ||
                       tok->is(TokenKind::unknown) ||
                       tok->is(TokenKind::comment);
            }

            virtual bool is_ignore() const {
                return default_ignore(current);
            }

            virtual void consume_hook(bool toroot) {}

            void consume_impl() {
                current = current->next;
                consume_hook(false);
            }

           public:
            void seek_root() {
                current = root;
                consume_hook(true);
            }

            bool consume() {
                if (!current) {
                    return false;
                }
                consume_impl();
                return true;
            }

            token_t& read() {
                while (current && is_ignore()) {
                    consume_impl();
                }
                return current;
            }

            token_t& consume_read() {
                if (!consume()) {
                    return current;
                }
                return read();
            }

            token_t& get() {
                return current;
            }

            token_t& consume_get() {
                if (!consume()) {
                    return current;
                }
                return get();
            }

            bool eos() const {
                return current == nullptr;
            }
        };
    }  // namespace tokenize
}  // namespace utils
