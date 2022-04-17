/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


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
            else if (kind == TokenKind::space) {
                Space<String>* space = cast<Space>(&in);
                assert(space);
                return BinaryIO::write_num(w, size_t(space->space)) &&
                       BinaryIO::write_num(w, size_t(space->count));
            }
            else if (kind == TokenKind::comment) {
                Comment<String>* cm = cast<Comment>(&in);
                assert(cm);
                return BinaryIO::write_num(w, size_t(cm->oneline)) &&
                       BinaryIO::write_string(cm->comment);
            }
            else if (kind == TokenKind::context) {
                PredefCtx<String>* ck = cast<PredefCtx>(&in);
                assert(ck);
                return BinaryIO::write_string(w, ck->token) &&
                       BinaryIO::write_string(w, ck->layer_);
            }
            else {
                Predef<String>* pd = cast<Predef>(&in);
                assert(pd);
                return BinaryIO::write_string(w, pd->token);
            }
        }
    }  // namespace bin_fmt
}  // namespace utils
