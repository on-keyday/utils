/*license*/

// token_list - token list format
#pragma once

#include "../tokenize/token.h"
#include "../tokenize/cast.h"
#include "../endian/writer.h"

namespace utils {
    namespace bin_fmt {
        template <class Buf, class String>
        bool write_token(endian::Writer<Buf>& w, const tokenize::Token<String>& in) {
            if (!in) {
                return false;
            }
            tokenize::TokenKind kind = in.kind();
            w.write_hton(std::uint8_t(kind));
            tokenize::cast<>();
        }
    }  // namespace bin_fmt
}  // namespace utils
