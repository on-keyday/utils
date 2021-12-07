/*license*/

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
            }
            return token.size() != 0;
        }

        template <class String>
        struct Identifier : Token<String> {
            String identifier;

            String to_string() const override {
                return identifier;
            }

            bool has(const String& str) const override {
                return identifier == str;
            }
        };
    }  // namespace tokenize
}  // namespace utils
