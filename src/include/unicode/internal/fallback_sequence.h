/*license*/

// back_sequence - fallback the Sequencer following utf rule
#pragma once

#include "../../core/sequencer.h"
#include "../convert.h"

namespace utils {
    namespace utf {
        namespace internal {
            template <class T>
            constexpr bool fallback(Sequencer<T>& seq) {
                if (seq.rptr == 0) {
                    return false;
                }
                using minbuf_t = Minibuffer<char32_t>;
                constexpr size_t offset = 4 / sizeof(typename BufferType<T>::char_type);
                static_assert(offset != 0, "too large char type");
                if (seq.current(-1) < 0x80 || offset == 1) {
                    return seq.backto();
                }
                size_t base = seq.rptr;
                if (seq.eos()) {
                    base--;
                }
                minbuf_t minbuf;
                for (size_t i = offset; i != 0; i--) {
                    seq.rptr = base - i;
                    if (convert_one(seq, minbuf)) {
                        if (offset == 2 && i == 2 && seq.rptr != base) {
                            continue;
                        }
                        seq.rptr = base - i;
                        return true;
                    }
                }
                return false;
            }
        }  // namespace internal
    }      // namespace utf
}  // namespace utils
