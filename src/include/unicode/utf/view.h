/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// view - low memory utf view
// trade-off: to reduce memory cost, pay time cost
#pragma once

#include "../../core/sequencer.h"
#include "minibuffer.h"
#include "convert.h"
#include "fallback_sequence.h"

namespace utils::unicode::utf {

    template <class Buf, class Char>
    class View {
        mutable Sequencer<buffer_t<Buf>> sequence;
        mutable Minibuffer<Char> view;
        mutable size_t viewptr = 0;
        mutable size_t virtual_ptr = 0;

        size_t converted_size = 0;

        constexpr bool fallback() const {
            return internal::fallback(sequence);
        }

        constexpr bool read_one() const {
            view.clear();
            return convert_one(sequence, view, false, false);
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
                    if (view.size() <= viewptr) {
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
                        if (sequence.rptr != 0) {
                            if (!fallback()) {
                                return false;
                            }
                        }
                        if (!read_one()) {
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
            resize();
        }

        constexpr void resize() {
            if (count_converted_size()) {
                read_one();
            }
            else {
                view.clear();
            }
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

    template <class T>
    using U8View = View<T, std::uint8_t>;

    template <class T>
    using U16View = View<T, char16_t>;

    template <class T>
    using U32View = View<T, char32_t>;
}  // namespace utils::unicode::utf
