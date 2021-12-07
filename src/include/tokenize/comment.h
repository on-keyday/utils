/*license*/

// comment - comment marger
#pragma once
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
            bool merge_until(String& merged, wrap::shared_ptr<Token<String>>& root, wrap::shared_ptr<Token<String>>& last, F&& f, const char*& abort) {
                auto p = root->next;
                size_t count = 0;
                for (; p; p = p->next) {
                    if (f(p, last, merged, abort)) {
                        return true;
                    }
                    if (abort) {
                        return false;
                    }
                    merged += p->to_string();
                }
                abort = "unexpected EOF";
                return false;
            }

            template <class String>
            bool is_symbol_or_keyword_and_(auto&& p, const String& str) {
                return (p->is(TokenKind::symbol) || p->is(TokenKind::keyword)) && p->has(str);
            }

            template <class String>
            int make_and_merge_comment(wrap::shared_ptr<Token<String>>& first, wrap::shared_ptr<Token<String>>& last, String& merged, bool oneline) {
                auto comment = wrap::make_shared<Comment<String>>();
                comment->comment = std::move(merged);
                first->next = comment;
                comment->prev = std::addressof(*first);
                if (last) {
                    comment->next = last;
                    last->prev = std::addressof(*comment);
                }
                first = last;
                return 1;
            }

            template <class String, class F>
            int merge_impl(const char*& errmsg, wrap::shared_ptr<Token<String>>& p, F&& rule, bool oneline = false) {
                String merged;
                wrap::shared_ptr<Token<String>> lastp;
                errmsg = nullptr;
                if (!merge_until(merged, p, lastp, rule, errmsg)) {
                    return -1;
                }
                if (p->next == lastp) {
                    p = lastp;
                    return 1;
                }
                return make_and_merge_comment(p, lastp, merged, oneline);
            }

        }  // namespace internal

        template <class String>
        int make_comment_between(const char*& errmsg, wrap::shared_ptr<Token<String>>& p, const String& begin, const String& end, bool nest = false) {
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
                return internal::merge_impl(errmsg, p, rule);
            }
            return 0;
        }

        template <class String>
        int make_comment_util_line(const char*& errmsg, wrap::shared_ptr<Token<String>>& p, const String& begin) {
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
                return internal::merge_impl(errmsg, p, rule, true);
            }
            return 0;
        }

        template <class String>
        int make_comment_with_escape(const char*& errmsg, wrap::shared_ptr<Token<String>>& p, const String& begin, const String& end, const String& escape, bool allowline = false) {
            if (internal::is_symbol_or_keyword_and_(p, begin)) {
                auto rule = [&](auto& p, auto& last, auto&, auto& abort) {
                    if (internal::is_symbol_or_keyword_and_(p, end)) {
                        if (internal::is_symbol_or_keyword_and_(p->prev, escape)) {
                            return false;
                        }
                        last = p;
                        return true;
                    }
                    else if (p->is(TokenKind::line)) {
                        if (!allowline) {
                            abort = "unexpected EOL";
                            return false;
                        }
                    }
                    return false;
                };
                return internal::merge_impl(errmsg, p, rule);
            }
            return 0;
        }

    }  // namespace tokenize
}  // namespace utils
