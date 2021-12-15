/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// keyword - define syntax keyword
#pragma once

namespace utils {
    namespace syntax {
        enum class KeyWord {
            id,
            keyword,
            symbol,
            indent,
            not_space,
            eof,
            eol,
            eos,
            bos,
            string,
            integer,
            number,
            literal_keyword,
            literal_symbol,
        };

        constexpr const char* keyword_str[] = {
            "ID",        // identifier
            "KEYWORD",   // any keyword
            "SYMBOL",    // any symbol
            "INDENT",    // indent
            "NOTSPACE",  // anything with out space and line
            "EOF",       // end of file
            "EOL",       // end of line
            "EOS",       // end of segment
            "BOS",       // begin of segment
            "STRING",    // string
            "INTEGER",   // integer
            "NUMBER",    // real number.

            // for literal
            "LITERAL_KEYWORD",  // keyword
            "LITERAL_SYMBOL",   // symbol
        };

        constexpr const char* keywordv(KeyWord kw) {
            auto v = static_cast<size_t>(kw);
            if (v >= sizeof(keyword_str) / sizeof(keyword_str[0])) {
                return "";
            }
            return keyword_str[v];
        }

    }  // namespace syntax
}  // namespace utils
