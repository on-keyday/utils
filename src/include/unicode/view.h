/*license*/

// view - low memory utf view
#pragma once

#include "../core/sequencer.h"
#include "minibuffer.h"
#include "convert.h"
#include "internal/fallback_sequence.h"

namespace utils {
    namespace utf {
        template <class Buf, class Char>
        class View {
            mutable Sequencer<Buf> sequence;
            mutable Minibuffer<Char> view;
            mutable size_t viewptr = 0;
            mutable size_t virtual_ptr = 0;

            size_t converted_size = 0;

            constexpr bool fallback() const {
                return internal::fallback(sequence);
            }

            constexpr bool read_one() const {
                if (sequence.eos()) {
                    return false;
                }
                view.clear();
                return convert_one(sequence, view);
            }

            constexpr bool count_converted_size() const {
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

            constexpr bool move_to(size_t position) const {
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
                return true;
            }

           public:
            template <class... Args>
            constexpr View(Args&&... args)
                : sequence(std::forward<Args>(args)...) {
            }

            constexpr size_t size() const {
                return converted_size;
            }

            constexpr Char operator[](size_t pos) const {
                if (!move_to(pos)) {
                    return Char();
                }
                return view[viewptr];
            }
        };
    }  // namespace utf
}  // namespace utils
