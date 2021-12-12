/*license*/

// token_list - token list format
#pragma once

#include "../../tokenize/token.h"
#include "../../tokenize/cast.h"
#include "../../endian/writer.h"
#include "../numstr.h"

#include <cassert>

namespace utils {
    namespace bin_fmt {
        template <class Buf, class String>
        bool write_token(endian::Writer<Buf>& w, tokenize::Token<String>& in) {
            if (!in) {
                return false;
            }
            using namespace tokenize;
            TokenKind kind = in.kind();
            w.write_hton(std::uint8_t(kind));
            if (kind == TokenKind::root || kind == TokenKind::unknown) {
                return true;
            }
            else if (kind == TokenKind::identifier) {
                Identifier<String>* id = cast<Identifier>(&in);
                assert(id);
                return BinaryIO::write_string(w, id->identifier);
            }
            else if (kind == TokenKind::line) {
                Line<String>* line = cast<Line>(&in);
                assert(line);
                return BinaryIO::write_num(w, size_t(line->line)) &&
                       BinaryIO::write_num(w, size_t(line->count));
            }
            else if (kind == TokenKind::context) {
                PredefCtx<String>* ck = cast<PredefCtx>(&in);
            }
        }
    }  // namespace bin_fmt
}  // namespace utils
