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
            endian::read_from(val, data, be);
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

        template <class T, class C, class U>
        constexpr bool read_aligned_num(basic_reader<C, U>& r, T& v, size_t align, bool be = true) {
            if (align == 0) {
                return false;
            }
            const auto offset = r.offset();
            auto ptr = [&] {
                return std::uintptr_t(r.remain().data());
            };
            while (!r.empty() && ptr() % align) {
                r.offset(1);
            }
            if (r.empty()) {
                r.reset(offset);
                return false;
            }
            auto res = read_num(r, v, be);
            if (!res) {
                r.reset(offset);
                return false;
            }
            return true;
        }

        template <class T, class C, class U>
        constexpr bool write_aligned_num(basic_writer<C, U>& w, T v, size_t align, bool be = true, bool fill = false, byte fill_byte = 0) {
            if (align == 0) {
                return false;
            }
            const auto offset = w.offset();
            auto ptr = [&] {
                return std::uintptr_t(w.remain().data());
            };
            while (!w.empty() && ptr() % align) {
                if (fill) {
                    w.write(fill_byte, 1);
                }
                else {
                    w.offset(1);
                }
            }
            if (w.empty()) {
                w.reset(offset);
                return false;
            }
            auto res = write_num(w, v, be);
            if (!res) {
                w.reset(offset);
                return false;
            }
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
