/*license*/

// view - low memory utf view
#pragma once

#include "../core/sequencer.h"
#include "minibuffer.h"
#include "convert.h"
#include "internal/fallback_sequence.h"

namespace utils {
    namespace utf {
        template <class Buf>
        class View {
            Sequencer<Buf> sequence;
            Minibuffer<char> view;
            size_t viewptr = 0;
            size_t converted_size = 0;
            size_t virtual_ptr = 0;

            constexpr bool fallback() {
                return internal::fallback(sequence);
            }

            constexpr bool read_one() {
                if (sequence.eos()) {
                    return false;
                }
                view.clear();
                return convert_one(sequence, view);
            }

            constexpr bool count_converted_size() {
                auto tmp = sequence.rptr;
                sequence.rptr = 0;
                size_t count = 0;
                while (!sequence.eos()) {
                    if (!read_one()) {
                        sequence.rptr = tmp;
                        converted_size = 0;
                        return false;
                    }
                    count += view.size();
                }
                converted_size = count;
                sequence.rptr = tmp;
                return true;
            }

            constexpr bool move_to(size_t position) {
                if (position == virtual_ptr) {
                    return true;
                }
                if (position >= converted_size) {
                    return false;
                }
                if (position > virtual_ptr) {
                    while (position != virtual_ptr) {
                        virtual_ptr++;
                        viewptr++;
                        if (view.size() >= viewptr) {
                            if (!read_one()) {
                                return false;
                            }
                            viewptr = 0;
                        }
                    }
                }
                else {
                    while (position != virtual_ptr) {
                        virtual_ptr--;
                        if (viewptr == 0) {
                            if (!fallback()) {
                                return false;
                            }
                            viewptr = view.size();
                        }
                        viewptr--;
                    }
                }
            }

           public:
        };
    }  // namespace utf
}  // namespace utils
