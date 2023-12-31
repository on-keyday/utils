/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// retreat - retreat the Sequencer following utf rule
#pragma once

#include "../../core/sequencer.h"
#include "convert.h"
#include "minibuffer.h"

namespace utils::unicode::utf::internal {

    template <class T>
    constexpr bool retreat(Sequencer<T>& seq) {
        if (seq.rptr == 0) {
            return false;
        }
        using minbuf_t = MiniBuffer<char32_t>;
        constexpr size_t offset = 4 / sizeof(typename BufferType<T>::char_type);
        static_assert(offset != 0, "too large char type");
        std::make_unsigned_t<decltype(seq.current())> c = seq.current(-1);
        if (c < 0x80 || offset == 1) {
            return seq.backto();
        }
        size_t base = seq.rptr;
        minbuf_t minbuf;
        for (size_t i = offset; i != 0; i--) {
            if (base < i) {
                continue;
            }
            seq.rptr = base - i;
            if (convert_one(seq, minbuf, false, false)) {
                /*
                    if seq.rptr is not returned to base,
                    it means that sequence from base - i
                    to current seq.rptr is not a part of 1 previous character (maybe two or more previous).
                    for example, when we try to retreat " あg" (space + hiragana a + g) in utf8
                    at the end of the sequence (position of g),
                    first try of base - i  will get " "  (space)
                    but this space is not a part of previous character (that means あ).
                    so we need this check to prevent this case
                */
                if (seq.rptr != base) {
                    continue;
                }
                seq.rptr = base - i;
                return true;
            }
        }
        return false;
    }
}  // namespace utils::unicode::utf::internal
