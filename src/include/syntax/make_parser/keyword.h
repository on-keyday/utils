/*license*/

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
            "NUMBER",    // real number
        };

        constexpr const char* keyword(KeyWord kw) {
            auto v = static_cast<size_t>(kw);
            if (v >= sizeof(keyword_str) / sizeof(keyword_str[0])) {
                return "";
            }
            return keyword_str[v];
        }

    }  // namespace syntax
}  // namespace utils
