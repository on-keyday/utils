/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// merger - define merge contexts
#pragma once
#include "merge.h"
#include <wrap/light/string.h>

namespace utils {
    namespace tokenize {

        template <class String = wrap::string>
        internal::CommentMergeContext<String> c_comment(bool nest = false) {
            return blcok_comment<String>("/*", "*/", nest);
        }

        template <class String = wrap::string>
        internal::CommentMergeContext<String> string(bool allow_line = false) {
            return escaped_comment<String>("\"", "\\", allow_line);
        }

        template <class String = wrap::string>
        internal::CommentMergeContext<String> cpp_comment() {
            return line_comment<String>("//");
        }

        template <class String = wrap::string>
        internal::CommentMergeContext<String> sh_comment() {
            return line_comment<String>("#");
        }

    }  // namespace tokenize
}  // namespace utils
