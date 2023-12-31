/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// number - numeric read write
#pragma once
#include "reader.h"
#include "writer.h"
#include "../binary/buf.h"
#include <utility>

namespace utils {
    namespace binary {
        template <class T, class C>
        constexpr bool read_num(basic_reader<C>& r, T& val, bool be = true) {
            auto [data, ok] = r.read(sizeof(T));
            if (!ok) {
                return false;
            }
            binary::read_from(val, data, be);
            return true;
        }

        namespace internal {
            template <size_t s>
            struct B {
                size_t index[s]{};
            };

            template <class... T>
            constexpr B<sizeof...(T)> bulk_array() {
                B<sizeof...(T)> b;
                size_t i = 0;
                size_t off = 0;
                auto f = [&](size_t s) {
                    b.index[i] = off;
                    i++;
                    off += s;
                };
                (..., f(sizeof(T)));
                return b;
            }
        }  // namespace internal

        template <class... T, class C>
        constexpr bool read_num_bulk(basic_reader<C>& r, bool be, T&... val) {
            constexpr size_t size = (... + sizeof(val));
            view::basic_rvec<C> data;
            if (!r.read(data, size)) {
                return false;
            }
            constexpr auto b = internal::bulk_array<T...>();
            size_t i = 0;
            auto bulk = [&](auto& v, size_t idx) {
                read_from(v, data.data() + b.index[idx], be);
                i++;
            };
            (..., bulk(val, i));
            return true;
        }

        template <class T, class C>
        constexpr bool write_num(basic_writer<C>& w, T val, bool be = true) {
            auto rem = w.remain();
            if (rem.size() < sizeof(T)) {
                return false;
            }
            write_into(rem, val, be);
            w.offset(sizeof(T));
            return true;
        }

        template <class... T, class C>
        constexpr bool write_num_bulk(basic_writer<C>& w, bool be, T... val) {
            constexpr auto size = (... + sizeof(val));
            auto rem = w.remain();
            if (rem.size() < size) {
                return false;
            }
            auto bulk = [&](auto& v, view::wvec& data) {
                binary::write_into(data, v, be);
                data = view::wvec(data.data() + sizeof(v), data.size() - sizeof(v));
            };
            (..., bulk(val, rem));
            w.offset(size);
            return true;
        }

        template <class C>
        constexpr bool write_uint24(basic_writer<C>& w, std::uint32_t val, bool be = true) {
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

        template <class C>
        constexpr bool read_uint24(basic_reader<C>& r, std::uint32_t& val, bool be = true) {
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

        template <class T, class C>
        constexpr bool read_aligned_num(basic_reader<C>& r, T& v, size_t align, bool be = true) {
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

        template <class T, class C>
        constexpr bool write_aligned_num(basic_writer<C>& w, T v, size_t align, bool be = true, bool fill = false, byte fill_byte = 0) {
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
                binary::writer w{data};
                std::uint32_t val = 0x9324f2;
                write_uint24(w, val);
                if (data[0] != 0x93 || data[1] != 0x24 || data[2] != 0xf2) {
                    return false;
                }
                binary::reader r{data};
                val = 0;
                read_uint24(r, val);
                return val == 0x9324f2;
            }

            constexpr bool check_bulk() {
                byte data[1 + 2 + 4 + 8];
                struct Bulk {
                    byte v1 = 1;
                    std::uint16_t v2 = 2;
                    std::uint32_t v3 = 3;
                    std::uint64_t v4 = 4;
                } def, b;
                writer w{data};
                if (!write_num_bulk(w, true, b.v1, b.v2, b.v3, b.v4)) {
                    return false;
                }
                b = {0, 0, 0, 0};
                if (b.v1 != 0 || b.v2 != 0 || b.v3 != 0 || b.v4 != 0) {
                    return false;
                }
                reader r{data};
                if (!read_num_bulk(r, true, b.v1, b.v2, b.v3, b.v4)) {
                    return false;
                }
                return def.v1 == b.v1 &&
                       def.v2 == b.v2 &&
                       def.v3 == b.v3 &&
                       def.v4 == b.v4;
            }

            static_assert(check_uint24());
            static_assert(check_bulk());
        }  // namespace test
    }      // namespace binary
}  // namespace utils
