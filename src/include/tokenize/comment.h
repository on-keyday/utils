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
        namespace internal {
            template <class String, class F>
            bool merge_until(String& merged, wrap::shared_ptr<Token<String>>& root, wrap::shared_ptr<Token<String>>& last, F&& f) {
                auto p = root->next;
                size_t count = 0;
                bool abort = false;
                for (; p; p = p->next) {
                    if (f(p, last, merged, abort)) {
                        return true;
                    }
                    if (abort) {
                        return false;
                    }
                    merged += p->to_string();
                }
                return false;
            }

            template <class String>
            bool is_symbol_or_keyword_and_(auto&& p, const String& str) {
                return (p->is(TokenKind::symbol) || p->is(TokenKind::keyword)) && p->has(str);
            }

            template <class String>
            int make_and_merge_comment(wrap::shared_ptr<Token<String>>& first, wrap::shared_ptr<Token<String>>& last, String& merged) {
                auto comment = = wrap::make_shared<Comment<String>>();
                comment->comment = std::move(merged);
                first->next = comment;
                comment->next = last;
                first = last;
                return 1;
            }

            template <class String, class F>
            int merge_impl(wrap::shared_ptr<Token<String>>& p, F&& rule) {
                String merged;
                wrap::shared_ptr<Token<String>> lastp;
                if (!merge_until(merged, p, lastp, rule)) {
                    return -1;
                }
                if (p->next == lastp) {
                    p = lastp;
                    return 1;
                }
                return make_and_merge_comment(p, lastp, merged);
            }

        }  // namespace internal

        template <class String>
        int make_comment_between(wrap::shared_ptr<Token<String>>& p, const String& begin, const String& end, bool nest = false) {
            if (internal::is_symbol_or_keyword_and_(p, begin)) {
                size_t count = 0;
                auto kind = p->kind();
                auto rule = [&](auto& p, auto& last, auto&, auto&) {
                    if (nest) {
                        if (p->is(kind) && p->has(begin)) {
                            count++;
                        }
                    }
                    if (internal::is_symbol_or_keyword_and_(p, end)) {
                        if (count == 0) {
                            last = p;
                            return true;
                        }
                        count--;
                    }
                    return false;
                };
                return internal::merge_impl(p, rule);
            }
            return 0;
        }

        template <class String>
        int make_comment_util_line(wrap::shared_ptr<Token<String>>& p, const String& begin) {
            if (internal::is_symbol_or_keyword_and_(p, begin)) {
                auto rule = [&](auto& p, auto& last, auto& merged, auto&) {
                    if (p->is(TokenKind::line)) {
                        last = p;
                        return true;
                    }
                    else if (!p->next) {
                        merged += p->to_string();
                        last = nullptr;
                        return true;
                    }
                    return false;
                };
                return internal::merge_impl(p, rule);
            }
            return 0;
        }

        template <class String>
        int make_comment_with_escape(wrap::shared_ptr<Token<String>>& p, const String& begin, const String& end, const String& escape, bool allowline = false) {
            if (internal::is_symbol_or_keyword_and_(p, begin)) {
                auto rule = [&](auto& p, auto& last, auto&, auto& abort) {
                    if (internal::is_symbol_or_keyword_and_(p, end)) {
                        if (internal::is_symbol_or_keyword_and_(p->prev, escape)) {
                            return false;
                        }
                        return true;
                    }
                    return false;
                };
                return internal::merge_impl();
            }
        }

    }  // namespace tokenize
}  // namespace utils
