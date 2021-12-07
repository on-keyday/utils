/*license*/

// comment - comment marger

#include "token.h"

namespace utils {
    namespace tokenize {

        template <class String>
        struct Comment : Token<String> {
            String comment;
            bool oneline = false;

            constexpr Comment()
                : Token<String>(TokenKind::comment) {}

            String to_string() const override {
                return comment;
            }

            bool has(const String& str) const override {
                return comment == str;
            }
        };

        template <class String, class F>
        bool merge_until(String& merged, wrap::shared_ptr<Token<String>>& root, F&& f) {
            auto p = root->next;
            size_t count = 0;
            for (; p; p = p->next) {
                if (f(p)) {
                    return true;
                }
                merged += p->to_string();
            }
            return false;
        }

        template <bool nest, class String>
        int make_comment_between(wrap::shared_ptr<Token<String>>& p, const String& begin, const String& end) {
            if (p->is(TokenKind::symbol) || p->is(TokenKind::keyword)) {
                if (p->has(begin)) {
                    wrap::shared_ptr<Token<String>> lastp;
                    String merged;
                    size_t count = 0;
                    auto kind = p->kind();
                    auto rule = [&](auto& p) {
                        if constexpr (next) {
                            if (p->is(kind) && p->has(begin)) {
                                count++;
                            }
                        }
                        if (p->is(TokenKind::symbol) || p->is(TokenKind::keyword)) {
                            if (p->has(end)) {
                                if (count == 0) {
                                    lastp = p;
                                    return true;
                                }
                                count--;
                            }
                        }
                        return false;
                    };
                    if (!merge_until(merged, p, rule)) {
                        return -1;
                    }
                    if (p->next == lastp) {
                        p = lastp;
                        return 1;
                    }
                    auto comment = = wrap::make_shared<Comment<String>>();
                    comment->comment = std::move(merged);
                    p->next = comment;
                    comment->next = lastp;
                    p = lastp;
                    return 1;
                }
            }
            return 0;
        }

        template <bool nest, class String>
        int make_comment_between(String& merged, wrap::shared_ptr<Token<String>>& p, const String& begin) {
        }

    }  // namespace tokenize
}  // namespace utils
