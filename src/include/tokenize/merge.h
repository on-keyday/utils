/*license*/

// merge - merge comment
#pragma once

#include "token.h"
#include "comment.h"

namespace utils {
    namespace tokenize {

        namespace internal {
            enum class CommentType {
                block = 0x1,
                oneline = 0x2,
                escape = 0x4,
                nest = 0x10,
                allow_line = 0x20,
            };

            DEFINE_ENUM_FLAGOP(CommentType)

            template <class String>
            struct CommentMergeContext {
                CommentType type;
                String begin;
                String end;
                String escape;
            };

            template <class String>
            int merge_impl(wrap::shared_ptr<Token<String>>& inout, CommentMergeContext<String>& ctx) {
                if (any(ctx.type & CommentType::block)) {
                    return make_comment_between(inout, ctx.begin, ctx.end, any(ctx.type & CommentType::nest));
                }
                else if (any(ctx.type & CommentType::oneline)) {
                    return make_comment_util_line(inout, ctx.begin);
                }
                else if (any(ctx.type & CommentType::escape)) {
                    return make_comment_with_escape(inout, ctx.begin, ctx.end, ctx.escape, any(ctx.type & CommentType::allow_line));
                }
                return -1;
            }

            template <class String>
            bool merge_each(wrap::shared_ptr<Token<String>>& inout) {
                return true;
            }

            template <class String, class Ctx, class... Ctxs>
            bool merge_each(wrap::shared_ptr<Token<String>>& inout, Ctx&& ctx, Ctxs&&... other) {
                auto res = merge_impl(inout, ctx);
                if (res < 0) {
                    return false;
                }
                if (res == 1) {
                    return true;
                }
                return merge_each(inout, std::forward<Ctxs>(other)...);
            }
        }  // namespace internal

        template <class String>
        internal::CommentMergeContext<String> blcok_comment(const String& begin, const String& end, bool nest = false) {
            internal::CommentType type = internal::CommentType::block;
            if (nest) {
                type |= internal::CommentType::nest;
            }
            return internal::CommentMergeContext<String>{
                .type = type,
                .begin = begin,
                .end = end,
            };
        }

        template <class String>
        internal::CommentMergeContext<String> line_comment(const String& begin) {
            return internal::CommentMergeContext<String>{
                .type = internal::CommentType::oneline,
                .begin = begin,
            };
        }

        template <class String, class... Ctx>
        bool merge(wrap::shared_ptr<Token<String>>& input, Ctx&&... ctx) {
            for (auto inout = input; inout; inout = inout->next) {
                if (!internal::merge_each(inout, std::forward<Ctx>(ctx)...)) {
                    return true;
                }
                if (!inout) {
                    break;
                }
            }
            return true;
        }

    }  // namespace tokenize
}  // namespace utils
