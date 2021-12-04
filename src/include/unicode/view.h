/*license*/

// view - low memory utf view
#pragma once

#include "../core/sequencer.h"
#include "minibuffer.h"
#include "convert.h"

namespace utils {
    namespace utf {
        template <class Buf>
        class View {
            Sequencer<Buf> sequence;
            Minibuffer<char> view;
            size_t viewptr = 0;
            size_t converted_size = 0;

            bool read_one() {
                if (sequence.eos()) {
                    return false;
                }
                view.clear();
                return convert_one(sequence, view);
            }

            bool count_converted_size(size_t& result) {
                auto tmp = sequence.rptr;
                sequence.rptr = 0;
                size_t count = 0;
                while (!sequence.eos()) {
                    if (!read_one()) {
                        sequence.rptr = tmp;
                        result = count;
                        return false;
                    }
                    count += view.size();
                }
                result = count;
                sequence.rptr = tmp;
                return true;
            }

           public:
        };
    }  // namespace utf
}  // namespace utils
