/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <binary/reader.h>
#include <binary/writer.h>
#include <binary/signint.h>

namespace futils::binary {
    template <class T, class C>
    constexpr bool read_leb128_uint(basic_reader<C>& r, T& result) {
        using U = uns_t<T>;
        result = 0;
        int shift = 0;
        for (;;) {
            if (r.empty()) {
                return false;
            }
            if (shift >= sizeof(T) * bit_per_byte) {
                return false;  // overflow
            }
            auto t = r.top();
            auto v = U(t & 0x7f);
            r.offset(1);
            result |= v << shift;
            if ((t & 0x80) == 0) {
                break;
            }
            shift += 7;
        }
        return true;
    }

    template <class T, class C>
    constexpr bool write_leb128_uint(basic_writer<C>& w, T value) {
        using U = uns_t<T>;
        U v = value;
        do {
            auto t = byte(v & 0x7f);
            v >>= 7;
            if (v != 0) {
                t |= 0x80;
            }
            if (!w.write(t, 1)) {
                return false;
            }
        } while (v != 0);
        return true;
    }

    template <class T>
    constexpr size_t len_leb128_uint(T value) {
        using U = uns_t<T>;
        U v = value;
        size_t i = 0;
        do {
            v >>= 7;
            i++;
        } while (v != 0);
        return i;
    }

    template <class T, class C>
    constexpr bool read_leb128_int(basic_reader<C>& r, T& result) {
        using U = uns_t<T>;
        result = 0;
        int shift = 0;
        constexpr auto size = sizeof(T) * bit_per_byte;
        bool sign = false;
        for (;;) {
            if (r.empty()) {
                return false;
            }
            if (shift >= size) {
                return false;  // overflow
            }
            auto t = r.top();
            auto v = U(t & 0x7f);

            r.offset(1);
            result |= v << shift;
            shift += 7;
            if ((t & 0x80) == 0) {
                sign = t & 0x40;
                break;
            }
        }
        if (shift < size && sign) {
            result |= (~U(0) << shift);
        }
        return true;
    }

    template <class T, class C>
    constexpr bool write_leb128_int(basic_writer<C>& w, T value) {
        using U = uns_t<T>;
        int shift = 0;
        constexpr auto size = sizeof(T) * bit_per_byte;
        bool sign = (value < 0);
        bool more = true;
        U v = value;
        while (more) {
            auto t = byte(v & 0x7f);
            v >>= 7;
            if (sign) {
                v |= (~U(0) << size - 7);
            }
            if ((v == 0 && (t & 0x40) == 0) ||
                (v == ~U(0) && (t & 0x40) != 0)) {
                more = false;
            }
            else {
                t |= 0x80;
            }
            if (!w.write(t, 1)) {
                return false;
            }
        }
        return true;
    }

    template <class T>
    constexpr size_t len_leb128_int(T value) {
        using U = uns_t<T>;
        int shift = 0;
        constexpr auto size = sizeof(T) * bit_per_byte;
        bool sign = (value < 0);
        bool more = true;
        U v = value;
        size_t i = 0;
        while (more) {
            auto t = byte(v & 0x7f);
            v >>= 7;
            i++;
            if (sign) {
                v |= (~U(0) << size - 7);
            }
            if ((v == 0 && (t & 0x40) == 0) ||
                (v == ~U(0) && (t & 0x40) != 0)) {
                more = false;
            }
        }
        return true;
    }

    namespace test {
        constexpr bool check_leb128() {
            byte buffer[4]{};
            writer w{buffer};
            reader r{buffer};
            if (!write_leb128_uint(w, 624485)) {
                return false;
            }
            if (buffer[0] != 0xE5 || buffer[1] != 0x8E || buffer[2] != 0x26) {
                return false;
            }
            std::uint32_t code = 0;
            if (!read_leb128_uint(r, code)) {
                return false;
            }
            if (code != 624485) {
                return false;
            }
            w.reset();
            r.reset();
            if (!write_leb128_int(w, -123456)) {
                return false;
            }
            if (buffer[0] != 0xC0 || buffer[1] != 0xBB || buffer[2] != 0x78) {
                return false;
            }
            std::int32_t code2 = 0;
            if (!read_leb128_int(r, code2)) {
                return false;
            }
            if (code2 != -123456) {
                return false;
            }
            return true;
        }

        static_assert(check_leb128());
    }  // namespace test
}  // namespace futils::binary
