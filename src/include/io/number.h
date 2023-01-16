/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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

        template <class C, class U>
        constexpr bool write_uint24(basic_writer<C, U>& w, std::uint32_t val, bool be = true) {
            if (val > 0xffffff) {
                return false;
            }
            auto rem = w.remain();
            if (rem.size() < 3) {
                return false;
            }
            rem[1] = (val >> 8) & 0xff;
            rem[be ? 0 : 2] = (val >> 16) & 0xff;
            rem[be ? 2 : 0] = val & 0xff;
            w.offset(3);
            return true;
        }

        template <class C, class U>
        constexpr bool read_uint24(basic_reader<C, U>& r, std::uint32_t& val, bool be = true) {
            auto [data, ok] = r.read(3);
            if (!ok) {
                return false;
            }
            std::uint32_t tmp = 0;
            tmp |= std::uint32_t(data[1]) << 8;
            tmp |= std::uint32_t(data[0]) << (be ? 16 : 0);
            tmp |= std::uint32_t(data[2]) << (be ? 0 : 16);
            val = tmp;
            return true;
        }

        namespace test {
            constexpr bool check_uint24() {
                byte data[3];
                io::writer w{data};
                std::uint32_t val = 0x9324f2;
                write_uint24(w, val);
                if (data[0] != 0x93 || data[1] != 0x24 || data[2] != 0xf2) {
                    return false;
                }
                io::reader r{data};
                val = 0;
                read_uint24(r, val);
                return val == 0x9324f2;
            }

            static_assert(check_uint24());
        }  // namespace test
    }      // namespace io
}  // namespace utils
