/*license*/

// back_sequence - fallback the Sequencer following utf rule
#pragma once

#include "../../core/sequencer.h"
#include "../convert.h"

namespace utils {
    namespace utf {
        namespace internal {

            template <class T>
            bool fallback_impl(Sequencer<T>& seq) {
                if (seq.rptr == 0) {
                    return false;
                }
                size_t base = seq.rptr;
                if (seq.current(-1) < 0x80) {
                    seq.backto();
                    return true;
                }
                using minbuf_t = Minibuffer<typename BufferType<T>::char_type>;
                minbuf_t minbuf;
                constexpr size_t offset = minbuf_t::bufsize;
                for (size_t i = offset; i != 0; i--) {
                    seq.rptr = base - offset;
                    if (convert_one(seq, minbuf)) {
                        seq.rptr = base - offset;
                        return true;
                    }
                }
                return false;
            }
        }  // namespace internal
    }      // namespace utf
}  // namespace utils
