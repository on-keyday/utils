/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// semantics_stream - token with semantics stream
#pragma once
#include "utf_stream.h"

namespace utils {
    namespace parser {
        namespace stream {
            template <class String, class Check>
            struct IdentifierParser {
                Check check;
                const char* record;
                Token parse(Input& input) {
                    Token err;
                    size_t offset_base = 0, offset = 0, index = 0;
                    String str;
                    if (!read_utf_string(input, &err,
                                         default_utf_callback(check, str, offset_base, offset, index))) {
                        return err;
                    }
                    err = check.endok();
                    if (has_err(err)) {
                        return err;
                    }
                    if (auto sem = get_semantics(input)) {
                    }
                }
            };
        }  // namespace stream
    }      // namespace parser
}  // namespace utils
