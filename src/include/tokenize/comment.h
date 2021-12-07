/*license*/

// comment - comment marger

#include "token.h"

namespace utils {
    namespace tokenize {

        template <bool nest, class String>
        bool merge_until(String& merged, wrap::shared_ptr<Token<String>>& root, wrap::shared_ptr<Token<String>>& lastmerged, wrap::shared_ptr<Token<String>>& lastp, const String& begin, const String& end) {
            auto prev = root;
            auto kind = root->kind;
            auto p = root->next;
            size_t count = 0;
            for (; p; p = p->next) {
                if constexpr (nest) {
                    if (p->is(kind) && p->has(begin)) {
                        count++;
                    }
                }
                if ((p->is(TokenKind::symbol) || p->is(TokenKind::keyword)) && p->has(end)) {
                    if (count == 0) {
                        lastmerged = prev;
                        lastp = p;
                        return true;
                    }
                    count--;
                }
                merged += p->to_string();
                prev = p;
            }
            return false;
        }

        template <bool nest, class String>
        int make_comment_between(String& merged, wrap::shared_ptr<Token<String>>& p, const String& begin, const String& end) {
            if (p->is(TokenKind::symbol) || p->is(TokenKind::keyword)) {
                if (p->has(begin)) {
                    wrap::shared_ptr<Token<String>> lastmerged, lastp;
                    if (!merge_until(merged, p, lastmerged, lastp, begin, end)) {
                        return -1;
                    }
                    if (p->next == lastp) {
                        p = lastp;
                        return 1;
                    }
                }
            }
            return 0;
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