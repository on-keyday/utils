/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../space/space.h"
#include "../../space/eol.h"
#include <memory>
#include "../../view/expand_iovec.h"
#include <vector>
#include "../../io/reader.h"

namespace utils::yaml::lexer {
    using String = view::expand_storage_vec<std::allocator<byte>>;

    enum class LexerError {
        none,
        reserved_indicator,
    };

    enum class TokenKind {
        block_seq_entry,  // -
        map_key,          // ?
        map_value,        // :
        flow_seq_entry,   // ,
        flow_seq_begin,   // [
        flow_seq_end,     // ]
        flow_map_begin,   // {
        flow_map_end,     // }
        comment,          // #
        anchor,           // &
        alias,            // *
        tag,              // !
        literal,          // |
        foleded,          // >
        single_quote,     // '
        double_quote,     // "
        directive,        // %
    };

    constexpr const char* indicators[] = {
        "-",
        "?",
        ":",
        ",",
        "[",
        "]",
        "{",
        "}",
        "#",
        "&",
        "*",
        "|",
        ">",
        "'",
        "\"",
        "%",
    };

    struct Token {
        TokenKind kind;
    };

    enum class Chomping {
        strip,
        clip,
        keep,
    };

    enum class Place {
        unspec,
        block_in,
        block_out,
        block_key,
        flow_in,
        flow_out,
        flow_key,
    };

    constexpr bool is_pritable(std::uint32_t code) noexcept {
        return code == 0x09 ||
               code == 0x0A ||
               code == 0x0D ||
               (0x20 <= code && code <= 0x7E) ||
               code == 0x85 ||
               (0xA0 <= code && code <= 0xD7FF) ||
               (0xE000 <= code && code <= 0xFFFD) ||
               (0x010000 <= code && code <= 0x10FFFF);
    }

    struct LexerCommon {
        io::reader& r;
        constexpr LexerCommon(io::reader& r)
            : r(r) {}

        constexpr auto remain() {
            return make_cpy_seq(r.remain());
        }

        constexpr view::rvec read(size_t expected) {
            auto [data, _] = r.read(expected);
            return data;
        }

        constexpr view::rvec expect(auto&& v) {
            auto seq = remain();
            if (seq.seek_if(v)) {
                return read(seq.rptr);
            }
            return {};
        }
    };

}  // namespace utils::yaml::lexer
