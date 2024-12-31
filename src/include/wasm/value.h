/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "error.h"
#include <binary/leb128.h>
#include <binary/float.h>
#include <binary/number.h>
#include <unicode/utf/convert.h>
#include <helper/pushbacker.h>
#include <helper/transform.h>

namespace futils::wasm {
    using helper::either::assign_to, helper::either::cast_to, helper::either::push_back_to, helper::either::empty_value;

    template <class T>
    constexpr result<T> parse_uint(binary::reader& r) {
        T n = 0;
        if (!binary::read_leb128_uint(r, n)) {
            return unexpect(Error::decode_uint);
        }
        return n;
    }

    template <class T>
    constexpr result<T> parse_int(binary::reader& r) {
        T n = 0;
        if (!binary::read_leb128_int(r, n)) {
            return unexpect(Error::decode_int);
        }
        return n;
    }

    template <class T>
    constexpr result<void> parse_uint(binary::reader& r, T& n) {
        if (!binary::read_leb128_uint(r, n)) {
            return unexpect(Error::decode_uint);
        }
        return {};
    }

    template <class T>
    constexpr result<void> parse_int(binary::reader& r, T& n) {
        if (!binary::read_leb128_int(r, n)) {
            return unexpect(Error::decode_int);
        }
        return {};
    }

    template <class T>
    constexpr result<void> render_uint(binary::writer& w, T n) {
        if (!binary::write_leb128_uint(w, n)) {
            return unexpect(Error::encode_uint);
        }
        return {};
    }

    template <class T>
    constexpr result<void> render_int(binary::writer& w, T n) {
        if (!binary::write_leb128_int(w, n)) {
            return unexpect(Error::encode_int);
        }
        return {};
    }

    constexpr result<byte> read_byte(binary::reader& r) {
        if (r.empty()) {
            return unexpect(Error::short_input);
        }
        auto val = r.top();
        r.offset(1);
        return val;
    }

    constexpr result<void> write_byte(binary::writer& w, byte v) {
        if (!w.write(v, 1)) {
            return unexpect(Error::short_buffer);
        }
        return {};
    }

    using Float32 = binary::SingleFloat;
    using Float64 = binary::DoubleFloat;

    constexpr result<Float32> parse_float32(binary::reader& r) {
        Float32 sf;
        if (!binary::read_num(r, sf.to_int(), false)) {
            return unexpect(Error::decode_float);
        }
        return sf;
    }

    constexpr result<Float64> parse_float64(binary::reader& r) {
        Float64 sf;
        if (!binary::read_num(r, sf.to_int(), false)) {
            return unexpect(Error::decode_float);
        }
        return sf;
    }

    constexpr result<void> render_float32(binary::writer& w, Float32 sf) {
        if (!binary::write_num(w, sf.to_int(), false)) {
            return unexpect(Error::encode_float);
        }
        return {};
    }

    constexpr result<void> render_float64(binary::writer& w, Float64 sf) {
        if (!binary::write_num(w, sf.to_int(), false)) {
            return unexpect(Error::encode_float);
        }
        return {};
    }

    constexpr result<view::rvec> parse_name(binary::reader& r) {
        return parse_uint<std::uint32_t>(r).and_then([&](std::uint32_t len) -> result<view::rvec> {
            auto [data, ok] = r.read(len);
            if (!ok) {
                return unexpect(Error::short_input);
            }
            if (!utf::convert<1, 4>(data, helper::nop, false, false)) {
                return unexpect(Error::decode_utf8);
            }
            return data;
        });
    }

    constexpr result<void> parse_name(binary::reader& r, view::rvec& data) {
        return parse_uint<std::uint32_t>(r).and_then([&](std::uint32_t len) -> result<void> {
            if (!r.read(data, len)) {
                return unexpect(Error::short_input);
            }
            if (!utf::convert<1, 4>(data, helper::nop, false, false)) {
                return unexpect(Error::decode_utf8);
            }
            return {};
        });
    }

    constexpr result<void> render_name(binary::writer& w, view::rvec name) {
        if (name.size() > ~std::uint32_t(0)) {
            return unexpect(Error::large_input);
        }
        if (!utf::convert<1, 4>(name, helper::nop, false, false)) {
            return unexpect(Error::encode_utf8);
        }
        return render_uint<std::uint32_t>(w, std::uint32_t(name.size())).and_then([&]() -> result<void> {
            if (!w.write(name)) {
                return unexpect(Error::short_buffer);
            }
            return {};
        });
    }

    constexpr result<void> parse_vec(binary::reader& r, auto&& elem) {
        return parse_uint<std::uint32_t>(r).and_then([&](auto len) -> result<void> {
            for (std::uint32_t i = 0; i < len; i++) {
                if (auto res = elem(i); !res) {
                    return unexpect(res.error());
                }
            }
            return {};
        });
    }

    constexpr result<void> render_byte_vec(binary::writer& w, view::rvec elem) {
        if (elem.size() > ~std::uint32_t(0)) {
            return unexpect(Error::large_input);
        }
        return render_uint(w, std::uint32_t(elem.size())).and_then([&]() -> result<void> {
            if (!w.write(elem)) {
                return unexpect(Error::short_buffer);
            }
            return {};
        });
    }

    constexpr result<void> parse_byte_vec(binary::reader& r, view::rvec& elem) {
        return parse_uint<std::uint32_t>(r).and_then([&](auto len) -> result<void> {
            if (!r.read(elem, len)) {
                return unexpect(Error::short_input);
            }
            return {};
        });
    }

    constexpr result<void> render_vec(binary::writer& w, auto len, auto&& elem) {
        if (len > ~std::uint32_t(0)) {
            return unexpect(Error::large_input);
        }
        return render_uint(w, std::uint32_t(len)).and_then([&]() -> result<void> {
            for (std::uint32_t i = 0; i < len; i++) {
                if (auto res = elem(i); !res) {
                    return unexpect(res.error());
                }
            }
            return {};
        });
    }

    using Index = std::uint32_t;

}  // namespace futils::wasm
