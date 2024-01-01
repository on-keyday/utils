/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// view - low memory utf view
// trade-off: to reduce memory cost, pay time cost
#pragma once

#include "../../core/sequencer.h"
#include "minibuffer.h"
#include "convert.h"
#include "retreat.h"

namespace futils::unicode::utf {

    template <class Buf, class Char>
    class View {
        mutable Sequencer<buffer_t<Buf>> seq;
        mutable MiniBuffer<Char> view;
        mutable size_t view_ptr = 0;
        mutable size_t virtual_ptr = 0;

        size_t converted_size = 0;

        constexpr bool retreat() const {
            return internal::retreat(seq);
        }

        constexpr bool read_one(bool forward = true) const {
            view.clear();
            auto save = seq.rptr;
            auto res = convert_one(seq, view, false, false);
            if (!forward) {
                seq.rptr = save;
            }
            return res;
        }

        constexpr bool count_converted_size() {
            auto tmp = seq.rptr;
            seq.rptr = 0;
            size_t count = 0;
            while (!seq.eos()) {
                if (!read_one()) {
                    seq.rptr = tmp;
                    converted_size = 0;
                    return false;
                }
                count += view.size();
            }
            converted_size = count;
            seq.rptr = tmp;
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
                    view_ptr++;
                    if (view.size() <= view_ptr) {
                        if (!read_one(true)) {
                            return false;
                        }
                        if (!read_one(false)) {
                            return false;
                        }
                        view_ptr = 0;
                    }
                }
            }
            else {
                while (position != virtual_ptr) {
                    if (view_ptr == 0) {
                        if (!retreat()) {
                            return false;
                        }
                        if (!read_one(false)) {
                            return false;
                        }
                        view_ptr = view.size();
                    }
                    view_ptr--;
                    virtual_ptr--;
                }
            }
            return true;
        }

       public:
        template <class... Args>
        constexpr View(Args&&... args)
            : seq(std::forward<Args>(args)...) {
            resize();
        }

        constexpr View(View&& a)
            : seq(std::move(a.seq)), view(std::move(a.view)), view_ptr(a.view_ptr), virtual_ptr(a.virtual_ptr), converted_size(a.converted_size) {
            a.converted_size = 0;
        }

        constexpr void resize() {
            if (count_converted_size()) {
                read_one(false);
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
            return view[view_ptr];
        }
    };

    template <class T>
    using U8View = View<T, std::uint8_t>;

    template <class T>
    using U16View = View<T, char16_t>;

    template <class T>
    using U32View = View<T, char32_t>;
}  // namespace futils::unicode::utf
