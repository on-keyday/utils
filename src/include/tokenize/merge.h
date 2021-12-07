/*license*/

// merge - merge comment
#pragma once

#include "token.h"

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
        }  // namespace internal

    }  // namespace tokenize
}  // namespace utils