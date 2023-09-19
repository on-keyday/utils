/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include <vector>
#include <optional>
#include <view/span.h>
#include <variant>
#include "error.h"
#include "value.h"

namespace utils::wasm::type {

    enum class Type : byte {
        i32 = 0x7f,
        i64 = 0x7e,
        f32 = 0x7d,
        f64 = 0x7c,
        v128 = 0x7b,
        funcref = 0x70,
        externref = 0x6f,
        func = 0x60,

        empty = 0x40,
    };

    constexpr bool is_numtype(Type t) noexcept {
        switch (t) {
            case Type::i32:
            case Type::i64:
            case Type::f32:
            case Type::f64:
                return true;
            default:
                return false;
        }
    }

    constexpr bool is_vectype(Type t) noexcept {
        switch (t) {
            case Type::v128:
                return true;
            default:
                return false;
        }
    }

    constexpr bool is_reftype(auto t) noexcept {
        switch (Type(t)) {
            case Type::funcref:
            case Type::externref:
                return true;
            default:
                return false;
        }
    }

    constexpr bool is_valtype(auto v) noexcept {
        auto t = Type(v);
        return is_numtype(t) || is_vectype(t) || is_reftype(t);
    }

    constexpr result<Type> parse_valtype(binary::reader& r) {
        return read_byte(r).and_then([&](byte typ) -> result<Type> {
            if (!is_valtype(typ)) {
                return unexpect(Error::unexpected_type);
            }
            return Type(typ);
        });
    }

    constexpr result<Type> parse_reftype(binary::reader& r) {
        return read_byte(r).and_then([&](byte typ) -> result<Type> {
            if (!is_reftype(typ)) {
                return unexpect(Error::unexpected_type);
            }
            return Type(typ);
        });
    }

    constexpr result<void> render_type(binary::writer& w, Type t) {
        return w.write(byte(t), 1) ? result<void>{} : unexpect(Error::short_buffer);
    }

    constexpr result<void> render_valtype(binary::writer& w, Type t) {
        if (!is_valtype(t)) {
            return unexpect(Error::unexpected_type);
        }
        return write_byte(w, byte(t));
    }

    constexpr result<void> render_reftype(binary::writer& w, Type t) {
        if (!is_reftype(t)) {
            return unexpect(Error::unexpected_type);
        }
        return write_byte(w, byte(t));
    }

    using ResultType = view::rspan<Type>;

    constexpr result<ResultType> parse_result_type(binary::reader& r) {
        view::rvec data;
        return parse_byte_vec(r, data)
            .and_then([&]() -> result<ResultType> {
                for (auto i : data) {
                    if (!is_valtype(Type(i))) {
                        return unexpect(Error::unexpected_type);
                    }
                }
                return ResultType(std::bit_cast<const Type*>(data.data()), data.size());
            });
    }

    constexpr result<void> render_result_type(binary::writer& w, ResultType typ) {
        if (typ.size() > 0xffffffff) {
            return unexpect(Error::large_input);
        }
        for (auto c : typ) {
            if (!is_valtype(Type(c))) {
                return unexpect(Error::unexpected_type);
            }
        }
        return render_byte_vec(w, view::rvec(std::bit_cast<const byte*>(typ.data()), typ.size()));
    }

    struct FunctionType {
        ResultType params;
        ResultType results;

        constexpr auto render(binary::writer& w) const {
            return render_type(w, Type::func)
                .and_then([&] { return render_result_type(w, params); })
                .and_then([&] { return render_result_type(w, results); });
        }
    };

    constexpr result<FunctionType> parse_function_type(binary::reader& r) {
        FunctionType typ;
        return read_byte(r)
            .and_then([&](byte t) -> result<ResultType> {
                if (Type(t) != Type::func) {
                    return unexpect(Error::unexpected_type);
                }
                return parse_result_type(r);
            })
            .and_then([&](ResultType&& v) {
                typ.params = v;
                return parse_result_type(r);
            })
            .transform([&](ResultType&& v) {
                typ.results = v;
                return typ;
            });
    }

    struct Limits {
        std::uint64_t minimum = 0;
        std::uint64_t maximum = 0xffffffffffffffff;
        byte flags = 0;

        enum : byte {
            limits_exist_max = 1,
            shared_memory = 2,
            memory_64 = 4,
        };

        constexpr result<void> render(binary::writer& w) const {
            return write_byte(w, flags)
                .and_then([&] {
                    return render_uint(w, minimum);
                })
                .and_then([&]() -> result<void> {
                    if (!(flags & limits_exist_max)) {
                        return {};
                    }
                    return render_uint(w, maximum);
                });
        }
    };

    constexpr result<Limits> parse_limits(binary::reader& r) {
        if (r.empty()) {
            return unexpect(Error::short_input);
        }
        Limits l;
        l.flags = r.top();
        r.offset(1);
        if (l.flags & Limits::memory_64) {
            return parse_uint(r, l.minimum)
                .and_then([&]() -> result<void> {
                    if (!(l.flags & Limits::limits_exist_max)) {
                        return {};
                    }
                    return parse_uint(r, l.maximum);
                })
                .transform([&] { return l; });
        }
        std::uint32_t mi, mx;
        return parse_uint(r, mi)
            .and_then([&]() -> result<void> {
                l.minimum = mi;
                if (!(l.flags & Limits::limits_exist_max)) {
                    return {};
                }
                return parse_uint(r, mx);
            })
            .transform([&] { l.maximum=mx; return l; });
    }

    using MemoryType = Limits;

    constexpr result<MemoryType> parse_memory_type(binary::reader& r) {
        return parse_limits(r);
    }

    struct TableType {
        Type reftype;
        Limits limits;

        constexpr result<void> render(binary::writer& w) const noexcept {
            return render_reftype(w, reftype).and_then([&] { return limits.render(w); });
        }
    };

    constexpr result<TableType> parse_table_type(binary::reader& r) {
        auto t = parse_reftype(r);
        if (!t) {
            return t.transform([](auto) { return TableType{}; });
        }
        return parse_limits(r).transform([&](Limits&& lim) {
            return TableType{*t, lim};
        });
    }

    struct GlobalType {
        Type valtype;
        bool mut = false;

        constexpr result<void> render(binary::writer& w) const {
            return render_valtype(w, valtype).and_then([&] {
                return write_byte(w, mut ? 1 : 0);
            });
        }
    };

    constexpr result<GlobalType> parse_global_type(binary::reader& r) {
        return parse_valtype(r).and_then([&](Type&& tb) -> result<GlobalType> {
            byte m = 0;
            if (!r.read(view::wvec(&m, 1))) {
                return unexpect(Error::short_input);
            }
            if (m != 0 && m != 1) {
                return unexpect(Error::unexpected_type);
            }
            return GlobalType{tb, m == 1};
        });
    }

    struct BlockType {
        std::variant<Type, std::uint32_t> val;

        constexpr result<void> render(binary::writer& w) const {
            if (auto t = std::get_if<0>(&val)) {
                if (!is_valtype(*t) && *t != Type::empty) {
                    return unexpect(Error::unexpected_type);
                }
                return render_type(w, *t);
            }
            return render_int<std::int64_t>(w, std::int64_t(std::get<1>(val)));
        }
    };

    constexpr result<BlockType> parse_block_type(binary::reader& r) {
        if (r.empty()) {
            return unexpect(Error::short_input);
        }
        auto ty = Type(r.top());
        if (is_valtype(ty) || ty == Type::empty) {
            r.offset(1);
            return BlockType{ty};
        }
        auto val = parse_int<std::int64_t>(r);
        if (!val) {
            return val.transform(empty_value<BlockType>());
        }
        if (*val < 0 || *val > 0xffffffff) {
            return unexpect(Error::large_int);
        }
        return BlockType{std::uint32_t(*val)};
    }

}  // namespace utils::wasm::type
