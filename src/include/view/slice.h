/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// slice - view slice
#pragma once
#include "../core/sequencer.h"
#include "../strutil/strutil.h"

namespace futils {
    namespace view {
        template <class T>
        struct Slice {
            Buffer<buffer_t<T>> buf;
            size_t start = 0;
            size_t end = 0;
            size_t stride = 1;

            template <class V>
            constexpr Slice(V&& t)
                : buf(std::forward<V>(t)) {}

            using char_type = std::remove_cvref_t<typename BufferType<T>::char_type>;

            constexpr char_type operator[](size_t index) const {
                size_t ofs = stride <= 1 ? 1 : stride;
                auto idx = ofs * index;
                auto ac = start + idx;
                if (ac >= end || ac >= buf.size()) {
                    return char_type{};
                }
                return buf.at(ac);
            }

            constexpr size_t size() const {
                size_t d = stride <= 1 ? 1 : stride;
                size_t ed = buf.size() <= end ? buf.size() : end;
                return ed > start ? (ed - start) / d : 0;
            }
        };

        template <class T>
        constexpr Slice<buffer_t<T&>> make_ref_slice(T&& in, size_t start, size_t end, size_t stride = 1) {
            Slice<buffer_t<T&>> slice{in};
            slice.start = start;
            slice.end = end;
            slice.stride = stride;
            return slice;
        }

        template <class T, class Sep = const char*>
        struct SplitView {
            Buffer<buffer_t<T>> buf;
            Sep sep;
            template <class V>
            constexpr SplitView(V&& t)
                : buf(std::forward<V>(t)) {}

            constexpr auto operator[](size_t index) const {
                constexpr auto eq = strutil::default_compare();
                size_t first = index == 0 ? 0 : strutil::find(buf.buffer, sep, index - 1, 0, eq);
                size_t second = strutil::find(buf.buffer, sep, index, 0, eq);
                if (first == ~0 && second == ~0) {
                    return make_ref_slice(buf.buffer, 0, 0);
                }
                auto add = make_ref_seq(sep).size();
                return make_ref_slice(buf.buffer, index == 0 ? first : first + add, second);
            }

            constexpr size_t size() const {
                return strutil::count(buf.buffer, sep) + 1;
            }
        };

        template <class T, class Sep>
        constexpr auto make_ref_splitview(T&& t, Sep&& sep) {
            SplitView<buffer_t<T&>, buffer_t<Sep>> splt{t};
            splt.sep = sep;
            return splt;
        }

        template <class T, class Sep>
        constexpr auto make_cpy_splitview(T&& t, Sep&& sep) {
            SplitView<buffer_t<std::remove_reference_t<T>>, buffer_t<Sep>> splt{std::forward<T>(t)};
            splt.sep = sep;
            return splt;
        }

    }  // namespace view
}  // namespace futils
