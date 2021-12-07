/*license*/

// comment - comment marger

#include "token.h"

namespace utils {
    namespace tokenize {
        template <class String>
        struct Comment : Token<String> {
            String comment;

            String to_string() const override {
                return comment;
            }

            bool has(const String& str) const override {
                return comment == str;
            }
        };
    }  // namespace tokenize
}  // namespace utils