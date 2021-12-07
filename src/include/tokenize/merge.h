/*license*/

// merge - merge comment
#pragma once

#include "token.h"
#include "comment.h"

namespace utils {
    namespace tokenize {
        enum class CommentType {
            block = 0x1,
            oneline = 0x2,
            escape = 0x4,
            nest = 0x10,
            allow_line = 0x20,
        };

        DEFINE_ENUM_FLAGOP(CommentType)

        namespace internal {
            template <class String>
            struct CommentMergeContext {
                CommentType type;
                String begin;
                String end;
                String escape;
            };

            template <class String>
            bool merge_impl(wrap::shared_ptr<Token<String>>& inout, CommentMergeContext<String>& ctx) {
                if (any(ctx.type & CommentType::block)) {
                    auto res = make_comment_between(inout, ctx.begin, ctx.end, any(ctx.type & CommentType::nest));
                    if (res < 0) {
                        return false;
                    }
                }
                else if (any(ctx.type & CommentType::oneline)) {
                    auto res = make_comment_util_line(inout, ctx.begin);
                    if (res < 0) {
                        return false;
                    }
                }
                else if (any(ctx.type & CommentType::escape)) {
                    auto res = make_comment_with_escape(inout, ctx.begin, ctx.end, ctx.escape, any(ctx.type & CommentType::allow_line));
                    if (res < 0) {
                        return false;
                    }
                }
                else {
                    return false;
                }
                return true;
            }
        }  // namespace internal

        template <class String>
        bool merge(wrap::shared_ptr<Token<String>>& input) {
        }

    }  // namespace tokenize
}  // namespace utils
