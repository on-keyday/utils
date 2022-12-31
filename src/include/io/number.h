/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// number - numeric read write
#pragma once
#include "reader.h"
#include "writer.h"
#include "../endian/buf.h"

namespace utils {
    namespace io {
        template <class T, class C, class U>
        constexpr bool read_num(basic_reader<C, U>& r, T& val, bool be = true) {
            auto [data, ok] = r.read(sizeof(T));
            if (!ok) {
                return false;
            }
            endian::read_from(val, data, be, 0);
            return true;
        }

        template <class T, class C, class U>
        constexpr bool write_num(basic_writer<C, U>& w, T val, bool be = true) {
            auto rem = w.remain();
            if (rem.size() < sizeof(T)) {
                return false;
            }
            endian::write_into(rem, val, be);
            w.offset(sizeof(T));
            return true;
        }
    }  // namespace io
}  // namespace utils
