/*license*/

// comment - comment marger

#include "token.h"

namespace utils {
    namespace tokenize {

        template <bool nest, class String>
        bool merge_until(String& merged, wrap::shared_ptr<Token<String>>& root, wrap::shared_ptr<Token<String>>& lastmerged, const String& begin, const String& end) {
            auto prev = root;

            for (auto p = root; p; p = p->next) {
            }
        }

        template <bool nest, class String>
        bool make_comment_between(wrap::shared_ptr<Token<String>>& p, const String& begin, const String& end) {
            if (p->is(TokenKind::symbol) || p->is(TokenKind::keyword)) {
                if (p->has(begin)) {
                }
            }
        }

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