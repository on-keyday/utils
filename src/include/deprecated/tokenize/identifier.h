/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// identifier - define what is identifier
#pragma once

#include "token.h"
#include "line.h"
#include "space.h"
#include "predefined.h"

namespace utils {
    namespace tokenize {

        template <class T, class String, class... Args>
        bool read_identifier(Sequencer<T>& seq, String& token, Args&&... args) {
            while (!seq.eos()) {
                if (!internal::is_not_separated<String>(seq, std::forward<Args>(args)...)) {
                    break;
                }
                token.push_back(seq.current());
                seq.consume();
            }
            return token.size() != 0;
        }

        template <class String>
        struct Identifier : Token<String> {
            String identifier;

            constexpr Identifier()
                : Token<String>(TokenKind::identifier) {}

            String to_string() const override {
                return identifier;
            }

            bool has(const String& str) const override {
                return identifier == str;
            }
        };
    }  // namespace tokenize
}  // namespace utils
