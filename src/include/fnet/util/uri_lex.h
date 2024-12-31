/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <number/char_range.h>

namespace futils::uri {

    // https://www.rfc-editor.org/rfc/rfc3986.html#appendix-A
    constexpr bool is_uri_unreserved(auto c) {
        // ALPHA / DIGIT / "-" / "." / "_" / "~"
        return number::is_in_byte_range(c) &&
               (number::is_alnum(c) || c == '-' || c == '.' || c == '_' || c == '~');
    }

    // https://www.rfc-editor.org/rfc/rfc3986.html#appendix-A
    constexpr bool is_uri_percent(auto c) {
        return c == '%';
    }
    // https://www.rfc-editor.org/rfc/rfc3986.html#appendix-A
    constexpr bool is_uri_gen_delims(auto c) {
        //  ":" / "/" / "?" / "#" / "[" / "]" / "@"
        return c == ':' || c == '/' || c == '?' || c == '#' ||
               c == '[' || c == ']' || c == '@';
    }

    // https://www.rfc-editor.org/rfc/rfc3986.html#appendix-A
    constexpr bool is_uri_sub_delims(auto c) {
        // "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
        return (c == '!' || c == '$' || c == '&' || c == '\'' ||
                c == '(' || c == ')' || c == '*' || c == '+' ||
                c == ',' || c == ';' || c == '=');
    }

    // https://www.rfc-editor.org/rfc/rfc3986.html#appendix-A
    constexpr bool is_uri_pchar(auto c) {
        // pchar         = unreserved / pct-encoded / sub-delims / ":" / "@"
        return is_uri_unreserved(c) || is_uri_percent(c) || is_uri_sub_delims(c) || c == ':' || c == '@';
    }

    // https://www.rfc-editor.org/rfc/rfc3986.html#appendix-A
    // for http path parser, we have to know if a character is usable in a uri, not to know structurally correct
    constexpr bool is_uri_usable(auto c) {
        return is_uri_pchar(c) || is_uri_gen_delims(c);
    }

}  // namespace futils::uri