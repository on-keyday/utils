/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "number.h"
#include "expandable_reader.h"
#include "expandable_writer.h"

namespace utils {
    namespace binary {
        template <class V, class T, class C>
        constexpr bool read_num(basic_expand_reader<T, C>& r, V& val, bool be = true) {
            if (!r.check_input(sizeof(V))) {
                return false;
            }
            auto br = r.reader();
            if (!read_num(br, val, be)) {
                return false;
            }
            r.offset(sizeof(T));
            return true;
        }

        template <class V, class T, class C>
        constexpr bool write_num(basic_expand_writer<T, C>& w, V val, bool be = true) {
            if (w.remain().size() < sizeof(V)) {
                w.expand(sizeof(V));
            }
            auto bw = w.writer();
            if (!write_num(bw, val, be)) {
                return false;
            }
            w.offset(sizeof(V));
            return true;
        }

        template <class T, class C>
        constexpr bool write_uint24(basic_expand_writer<T, C>& w, std::uint32_t val, bool be = true) {
            if (w.remain().size() < 3) {
                w.expand(3);
            }
            auto bw = w.writer();
            if (!write_uint24(bw, val, be)) {
                return false;
            }
            w.offset(3);
            return true;
        }

        template <class T, class C>
        constexpr bool read_uint24(basic_expand_reader<T, C>& r, std::uint32_t& val, bool be = true) {
            if (!r.check_input(3)) {
                return false;
            }
            auto rw = r.reader();
            if (!read_uint24(rw, val, be)) {
                return false;
            }
            r.offset(3);
            return true;
        }

    }  // namespace binary
}  // namespace utils