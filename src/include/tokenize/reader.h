/*license*/

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

            void seek_root() {
                root = current;
            }

           protected:
            static bool default_ignore(token_t& tok) {
                return tok->is(TokenKind::root) ||
                       tok->is(TokenKind::space) ||
                       tok->is(TokenKind::line) ||
                       tok->is(TokenKind::unknown) ||
                       tok->is(TokenKind::comment);
            }

            virtual bool is_ignore() {
                return default_ignore(current);
            }

            virtual void consume_hook() {}

            void consume_impl() {
                current = current->next;
                consume_hook();
            }

           public:
            bool consume() {
                if (!current) {
                    return false;
                }
                consume_impl();
                return true;
            }

            token_t& read() {
                if (current && is_ignore()) {
                    consume_impl();
                }
                return current;
            }

            token_t& get() {
                return current;
            }

            bool eos() const {
                return current == nullptr;
            }
        };
    }  // namespace tokenize
}  // namespace utils
