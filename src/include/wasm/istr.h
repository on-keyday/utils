/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "type.h"
#include "error.h"
#include "value.h"
#include <helper/expected_op.h>

namespace utils::wasm::code {
    enum class Instruction {
        unreachable = 0x00,
        nop = 0x01,
        block = 0x02,
        loop = 0x3,
        if_ = 0x04,
        else_ = 0x05,
        end = 0x0b,
        br = 0x0c,
        br_if = 0x0d,
        br_table = 0x0e,
        return_ = 0x0f,
        call = 0x10,
        call_indirect = 0x11,

        ref_null = 0xd0,
        ref_is_null = 0xd1,
        ref_func = 0xd2,

        drop = 0x1a,
        select = 0x1b,
        select_t = 0x1c,

        local_get = 0x20,
        local_set = 0x21,
        local_tee = 0x22,
        global_get = 0x23,
        global_set = 0x24,

        table_get = 0x25,
        table_set = 0x26,

        i32_load = 0x28,
        i64_load = 0x29,
        f32_load = 0x2a,
        f64_load = 0x2b,

        i32_load8_s = 0x2c,
        i32_load8_u = 0x2d,
        i32_load16_s = 0x2e,
        i32_load16_u = 0x2f,

        i64_load8_s = 0x30,
        i64_load8_u = 0x31,
        i64_load16_s = 0x32,
        i64_load16_u = 0x33,

        i64_load32_s = 0x34,
        i64_load32_u = 0x35,

        i32_store = 0x36,
        i64_store = 0x37,
        f32_store = 0x38,
        f64_store = 0x39,

        i32_store8 = 0x3a,
        i32_store16 = 0x3b,

        i64_store8 = 0x3c,
        i64_store16 = 0x3d,
        i64_store32 = 0x3e,

        memory_size = 0x3f,
        memory_grow = 0x40,

        i32_const = 0x41,
        i64_const = 0x42,
        f32_const = 0x43,
        f64_const = 0x44,

        i32_eqz = 0x45,
        i32_eq = 0x46,
        i32_ne = 0x47,
        i32_lt_s = 0x48,
        i32_lt_u = 0x49,
        i32_gt_s = 0x4a,
        i32_gt_u = 0x4b,
        i32_le_s = 0x4c,
        i32_le_u = 0x4d,
        i32_ge_s = 0x4e,
        i32_ge_u = 0x4f,

        i64_eqz = 0x50,
        i64_eq = 0x51,
        i64_ne = 0x52,
        i64_lt_s = 0x53,
        i64_lt_u = 0x54,
        i64_gt_s = 0x55,
        i64_gt_u = 0x56,
        i64_le_s = 0x57,
        i64_le_u = 0x58,
        i64_ge_s = 0x59,
        i64_ge_u = 0x5a,

        f32_eq = 0x5b,
        f32_ne = 0x5c,
        f32_lt = 0x5d,
        f32_gt = 0x5e,
        f32_le = 0x5f,
        f32_ge = 0x60,

        f64_eq = 0x61,
        f64_ne = 0x62,
        f64_lt = 0x63,
        f64_gt = 0x64,
        f64_le = 0x65,
        f64_ge = 0x66,

        i32_clz = 0x67,
        i32_ctz = 0x68,
        i32_popcnt = 0x69,
        i32_add = 0x6a,
        i32_sub = 0x6b,
        i32_mul = 0x6c,
        i32_div_s = 0x6d,
        i32_div_u = 0x6e,
        i32_rem_s = 0x6f,
        i32_rem_u = 0x70,
        i32_and = 0x71,
        i32_or = 0x72,
        i32_xor = 0x73,
        i32_shl = 0x74,
        i32_shr_s = 0x75,
        i32_shr_u = 0x76,
        i32_rotl = 0x77,
        i32_rotr = 0x78,

        i64_clz = 0x79,
        i64_ctz = 0x7a,
        i64_popcnt = 0x7b,
        i64_add = 0x7c,
        i64_sub = 0x7d,
        i64_mul = 0x7e,
        i64_div_s = 0x7f,
        i64_div_u = 0x80,
        i64_rem_s = 0x81,
        i64_rem_u = 0x82,
        i64_and = 0x83,
        i64_or = 0x84,
        i64_xor = 0x85,
        i64_shl = 0x86,
        i64_shr_s = 0x87,
        i64_shr_u = 0x88,
        i64_rotl = 0x89,
        i64_rotr = 0x8a,

        f32_abs = 0x8b,
        f32_neg = 0x8c,
        f32_ceil = 0x8d,
        f32_floor = 0x8e,
        f32_trunc = 0x8f,
        f32_nearest = 0x90,
        f32_sqrt = 0x91,
        f32_add = 0x92,
        f32_sub = 0x93,
        f32_mul = 0x94,
        f32_div = 0x95,
        f32_min = 0x96,
        f32_max = 0x97,
        f32_copysign = 0x98,

        f64_abs = 0x99,
        f64_neg = 0x9a,
        f64_ceil = 0x9b,
        f64_floor = 0x9c,
        f64_trunc = 0x9d,
        f64_nearest = 0x9e,
        f64_sqrt = 0x9f,
        f64_add = 0xa0,
        f64_sub = 0xa1,
        f64_mul = 0xa2,
        f64_div = 0xa3,
        f64_min = 0xa4,
        f64_max = 0xa5,
        f64_copysign = 0xa6,

        i32_wrap_i64 = 0xa7,
        i32_trunc_f32_s = 0xa8,
        i32_trunc_f32_u = 0xa9,
        i32_trunc_f64_s = 0xaa,
        i32_trunc_f64_u = 0xab,
        i64_extend_i32_s = 0xac,
        i64_extend_i32_u = 0xad,
        i64_trunc_f32_s = 0xae,
        i64_trunc_f32_u = 0xaf,
        i64_trunc_f64_s = 0xb0,
        i64_trunc_f64_u = 0xb1,
        f32_convert_i32_s = 0xb2,
        f32_convert_i32_u = 0xb3,
        f32_convert_i64_s = 0xb4,
        f32_convert_i64_u = 0xb5,
        f32_demote_f64 = 0xb6,
        f64_convert_i32_s = 0xb7,
        f64_convert_i32_u = 0xb8,
        f64_convert_i64_s = 0xb9,
        f64_convert_i64_u = 0xba,
        f64_promote_f32 = 0xbb,

        i32_reinterpret_f32 = 0xbc,
        i64_reinterpret_f64 = 0xbd,
        f32_reinterpret_i32 = 0xbe,
        f64_reinterpret_i64 = 0xbf,

        i32_extend8_s = 0xC0,
        i32_extend16_s = 0xC1,
        i64_extend8_s = 0xC2,
        i64_extend16_s = 0xC3,
        i64_extend32_s = 0xC4,

        i32_trunc_sat_f32_u = 0xfc01,
        i32_trunc_sat_f64_s = 0xfc02,
        i32_trunc_sat_f64_u = 0xfc03,
        i64_trunc_sat_f32_s = 0xfc04,
        i64_trunc_sat_f32_u = 0xfc05,
        i64_trunc_sat_f64_s = 0xfc06,
        i64_trunc_sat_f64_u = 0xfc07,

        memory_init = 0xfc08,
        data_drop = 0xfc09,
        memory_copy = 0xfc0a,
        memory_fill = 0xfc0b,

        table_init = 0xfc0c,
        elem_drop = 0xfc0d,

        table_copy = 0xfc0e,
        table_grow = 0xfc0f,
        table_size = 0xfc10,
        table_fill = 0xfc11,

        i8x16_swizzle = 0xFD0E,
        i8x16_splat = 0xFD0F,
        i16x8_splat = 0xFD10,
        i32x4_splat = 0xFD11,
        i64x2_splat = 0xFD12,
        f32x4_splat = 0xFD13,
        f64x2_splat = 0xFD14,

        i8x16_eq = 0xFD23,
        i8x16_ne = 0xFD24,
        i8x16_lt_s = 0xFD25,
        i8x16_lt_u = 0xFD26,
        i8x16_gt_s = 0xFD27,
        i8x16_gt_u = 0xFD28,
        i8x16_le_s = 0xFD29,
        i8x16_le_u = 0xFD2A,
        i8x16_ge_s = 0xFD2B,
        i8x16_ge_u = 0xFD2C,
        i16x8_eq = 0xFD2D,
        i16x8_ne = 0xFD2E,
        i16x8_lt_s = 0xFD2F,
        i16x8_lt_u = 0xFD30,
        i16x8_gt_s = 0xFD31,
        i16x8_gt_u = 0xFD32,
        i16x8_le_s = 0xFD33,
        i16x8_le_u = 0xFD34,
        i16x8_ge_s = 0xFD35,
        i16x8_ge_u = 0xFD36,

        i32x4_eq = 0xFD37,
        i32x4_ne = 0xFD38,
        i32x4_lt_s = 0xFD39,
        i32x4_lt_u = 0xFD3A,
        i32x4_gt_s = 0xFD3B,
        i32x4_gt_u = 0xFD3C,
        i32x4_le_s = 0xFD3D,
        i32x4_le_u = 0xFD3E,
        i32x4_ge_s = 0xFD3F,
        i32x4_ge_u = 0xFD40,

        i64x2_eq = 0xFDD6,
        i64x2_ne = 0xFDD7,
        i64x2_lt_s = 0xFDD8,
        i64x2_gt_s = 0xFDD9,
        i64x2_le_s = 0xFDDA,
        i64x2_ge_s = 0xFDDB,

        f32x4_eq = 0xFD41,
        f32x4_ne = 0xFD42,
        f32x4_lt = 0xFD43,
        f32x4_gt = 0xFD44,
        f32x4_le = 0xFD45,
        f32x4_ge = 0xFD46,
        f64x2_eq = 0xFD47,
        f64x2_ne = 0xFD48,
        f64x2_lt = 0xFD49,
        f64x2_gt = 0xFD4A,
        f64x2_le = 0xFD4B,
        f64x2_ge = 0xFD4C,
        v128_not = 0xFD4D,
        v128_and = 0xFD4E,
        v128_andnot = 0xFD4F,
        v128_or = 0xFD50,
        v128_xor = 0xFD51,
        v128_bitselect = 0xFD52,
        v128_any_true = 0xFD53,
        i8x16_abs = 0xFD54,
        i8x16_neg = 0xFD55,
        i8x16_popcnt = 0xFD56,
        i8x16_all_true = 0xFD57,
        i8x16_bitmask = 0xFD58,
        i8x16_narrow_i16x8_s = 0xFD59,
        i8x16_narrow_i16x8_u = 0xFD5A,
        i8x16_shl = 0xFD5B,
        i8x16_shr_s = 0xFD5C,
        i8x16_shr_u = 0xFD5D,
        i8x16_add = 0xFD5E,
        i8x16_add_sat_s = 0xFD5F,
        i8x16_add_sat_u = 0xFD60,
        i8x16_sub = 0xFD61,
        i8x16_sub_sat_s = 0xFD62,
        i8x16_sub_sat_u = 0xFD63,
        i8x16_min_s = 0xFD64,
        i8x16_min_u = 0xFD65,
        i8x16_max_s = 0xFD66,
        i8x16_max_u = 0xFD67,
        i8x16_avgr_u = 0xFD68,
        // TODO(on-keyday): more but tired

        need_more_1 = 0xFC,
        need_more_2 = 0xFD,
    };

    struct Op;

    struct Expr {
        std::vector<Op> ops;
    };

    struct Block {
        type::BlockType type;
        Expr body;
    };

    struct If {
        type::BlockType type;
        Expr block;
        std::optional<Expr> else_;
    };

    struct LabelTable {
        std::vector<Index> labels;
        Index label = 0;
    };

    struct Index2 {
        Index index1 = 0;
        Index index2 = 0;
    };

    struct Mem {
        std::uint32_t align = 0;
        std::uint32_t offset = 0;
    };

    result<void> parse_2uint(binary::reader& r, std::uint32_t& a, std::uint32_t& b) {
        return parse_uint(r, a).and_then([&] { return parse_uint(r, b); });
    }

    result<void> render_2uint(binary::writer& w, std::uint32_t a, std::uint32_t b) {
        return render_uint(w, a).and_then([&] { return render_uint(w, b); });
    }

    struct Literal {
        std::variant<std::int32_t, std::int64_t, Float32, Float64> literal;

        template <class T>
        result<const T*> as() const {
            auto res = std::get_if<T>(&literal);
            if (!res) {
                return unexpect(Error::unexpected_instruction_arg);
            }
            return res;
        }
    };

    using Arg = std::variant<std::monostate, Block, If,
                             Index, LabelTable, Index2, type::Type,
                             type::ResultType, Mem, Literal>;

    struct Op {
        Instruction instr = Instruction::nop;
        Arg arg;

        template <class T>
        result<const T*> as() const {
            auto res = std::get_if<T>(&arg);
            if (!res) {
                return unexpect(Error::unexpected_instruction_arg);
            }
            return res;
        }
    };

    inline result<Op> parse_instr(binary::reader& r);

    inline result<Expr> parse_expr(binary::reader& r, bool detect_else = false) {
        Expr b;
        while (true) {
            auto op = parse_instr(r);
            if (!op) {
                return op.transform([&](auto&&) { return std::move(b); });
            }
            b.ops.push_back(std::move(*op));
            if (b.ops.back().instr == Instruction::end ||
                (detect_else && b.ops.back().instr == Instruction::else_)) {
                break;
            }
        }
        return b;
    }

    inline result<void> render_instr(const Op& op, binary::writer& w);
    inline result<void> render_expr(const Expr& expr, binary::writer& w, bool should_be_else = false) {
        if (expr.ops.size() == 0 ||
            (should_be_else ? expr.ops.back().instr != Instruction::end
                            : expr.ops.back().instr != Instruction::else_)) {
            return unexpect(Error::unexpected_instruction);
        }
        for (auto& op : expr.ops) {
            if (auto res = render_instr(op, w); !res) {
                return res;
            }
        }
        return {};
    }

    inline result<void> render_block(const Block& b, binary::writer& w, bool should_be_else = false) {
        return b.type.render(w).and_then([&] { return render_expr(b.body, w, should_be_else); });
    }

    inline result<Block> parse_block(binary::reader& r, bool detect_else = false) {
        return type::parse_block_type(r).and_then([&](type::BlockType&& typ) -> result<Block> {
            auto expr = parse_expr(r, detect_else);
            if (!expr) {
                return expr.transform(empty_value<Block>());
            }
            return Block{std::move(typ), std::move(*expr)};
        });
    }

    inline result<void> render_if(const If& b, binary::writer& w) {
        return b.type.render(w)
            .and_then([&] { return render_expr(b.block, w, b.else_.has_value()); })
            .and_then([&] {
                if (!b.else_) {
                    return result<void>{};
                }
                return render_expr(*b.else_, w);
            });
    }

    inline result<If> parse_if(binary::reader& r) {
        return parse_block(r, true)
            .and_then([&](Block&& b) -> result<If> {
                if (b.body.ops.back().instr == Instruction::else_) {
                    auto res = parse_expr(r);
                    if (!res) {
                        return res.transform(empty_value<If>());
                    }
                    return If{std::move(b.type), std::move(b.body), std::move(*res)};
                }
                return If{std::move(b.type), std::move(b.body)};
            });
    }

    inline result<LabelTable> parse_label_table(binary::reader& r) {
        LabelTable table;
        return parse_vec(r, [&](auto i) {
                   return parse_uint<std::uint32_t>(r).transform(push_back_to(table.labels));
               })
            .and_then([&] { return parse_uint(r, table.label); })
            .transform([&] { return table; });
    }

    inline result<void> render_label_table(const LabelTable& l, binary::writer& w) {
        return render_vec(w, l.labels.size(), [&](auto i) {
                   return render_uint(w, l.labels[i]);
               })
            .and_then([&] {
                return render_uint(w, l.label);
            });
    }

    // these instruction have reserved for future area
    inline result<Op> parse_memory_xxx_instruction_arg(Instruction instr, binary::reader& r) {
        switch (instr) {
            case Instruction::memory_size:
            case Instruction::memory_grow:
            case Instruction::memory_fill: {
                return read_byte(r).and_then([&](byte v) -> result<Op> {
                    if (v != 0) {
                        return unexpect(Error::unexpected_instruction_arg);
                    }
                    return Op{instr};
                });
            }
            case Instruction::memory_copy: {
                return read_byte(r)
                    .and_then([&](byte v) -> result<byte> {
                        if (v != 0) {
                            return unexpect(Error::unexpected_instruction_arg);
                        }
                        return read_byte(r);
                    })
                    .and_then([&](byte v) -> result<Op> {
                        if (v != 0) {
                            return unexpect(Error::unexpected_instruction);
                        }
                        return Op{instr};
                    });
            }
            case Instruction::memory_init: {
                std::uint32_t index = 0;
                return parse_uint(r, index)
                    .and_then([&] {
                        return read_byte(r);
                    })
                    .and_then([&](byte b) -> result<Op> {
                        if (b != 0) {
                            return unexpect(Error::unexpected_instruction_arg);
                        }
                        return Op{instr, index};
                    });
            }
            default:
                return unexpect(Error::unexpected_instruction);
        }
    }

    // these instruction have reserved for future area
    inline result<void> render_memory_xxx_instruction_arg(const Op& op, binary::writer& w) {
        auto write_zero = [&] { return write_byte(w, 0); };
        switch (op.instr) {
            case Instruction::memory_size:
            case Instruction::memory_grow:
            case Instruction::memory_fill: {
                return write_zero();
            }
            case Instruction::memory_copy: {
                return write_zero() & write_zero;
            }
            case Instruction::memory_init: {
                op.as<Index>().and_then([&](auto b) {
                    return render_uint(w, *b) & write_zero;
                });
            }
            default:
                return unexpect(Error::unexpected_instruction);
        }
    }

    constexpr auto convert_to_Op(Instruction instr) {
        return [=](auto&& v) {
            return Op{instr, std::move(v)};
        };
    }

    constexpr result<Literal> parse_literal(Instruction instr, binary::reader& r) {
        auto to_literal = [](auto v) { return Literal{v}; };
        switch (instr) {
            case Instruction::i32_const: {
                return parse_int<std::int32_t>(r) & to_literal;
            }
            case Instruction::i64_const: {
                return parse_int<std::int64_t>(r) & to_literal;
            }
            case Instruction::f32_const: {
                return parse_float32(r) & to_literal;
            }
            case Instruction::f64_const: {
                return parse_float64(r) & to_literal;
            }
            default: {
                return unexpect(Error::unexpected_instruction);
            }
        }
    }

    constexpr result<void> render_literal(const Op& op, binary::writer& w) {
        return op.as<Literal>().and_then([&](const Literal* l) -> result<void> {
            switch (op.instr) {
                case Instruction::i32_const: {
                    return l->as<std::int32_t>().and_then([&](auto v) {
                        return render_int(w, *v);
                    });
                }
                case Instruction::i64_const: {
                    return l->as<std::int64_t>().and_then([&](auto v) {
                        return render_int(w, *v);
                    });
                }
                case Instruction::f32_const: {
                    return l->as<Float32>().and_then([&](auto v) {
                        return render_float32(w, *v);
                    });
                }
                case Instruction::f64_const: {
                    return l->as<Float64>().and_then([&](auto v) {
                        return render_float64(w, *v);
                    });
                }
                default: {
                    return unexpect(Error::unexpected_instruction);
                }
            }
        });
    }

#define NO_ARG_CASE                                   \
    Instruction::unreachable : case Instruction::nop: \
    case Instruction::return_:                        \
    case Instruction::end:                            \
    case Instruction::else_:                          \
    case Instruction::ref_is_null:                    \
    case Instruction::drop:                           \
    case Instruction::select:                         \
    case Instruction::i32_eqz:                        \
    case Instruction::i32_eq:                         \
    case Instruction::i32_ne:                         \
    case Instruction::i32_lt_s:                       \
    case Instruction::i32_lt_u:                       \
    case Instruction::i32_gt_s:                       \
    case Instruction::i32_gt_u:                       \
    case Instruction::i32_le_s:                       \
    case Instruction::i32_le_u:                       \
    case Instruction::i32_ge_s:                       \
    case Instruction::i32_ge_u:                       \
    case Instruction::i64_eqz:                        \
    case Instruction::i64_eq:                         \
    case Instruction::i64_ne:                         \
    case Instruction::i64_lt_s:                       \
    case Instruction::i64_lt_u:                       \
    case Instruction::i64_gt_s:                       \
    case Instruction::i64_gt_u:                       \
    case Instruction::i64_le_s:                       \
    case Instruction::i64_le_u:                       \
    case Instruction::i64_ge_s:                       \
    case Instruction::i64_ge_u:                       \
    case Instruction::f32_eq:                         \
    case Instruction::f32_ne:                         \
    case Instruction::f32_lt:                         \
    case Instruction::f32_gt:                         \
    case Instruction::f32_le:                         \
    case Instruction::f32_ge:                         \
    case Instruction::f64_eq:                         \
    case Instruction::f64_ne:                         \
    case Instruction::f64_lt:                         \
    case Instruction::f64_gt:                         \
    case Instruction::f64_le:                         \
    case Instruction::f64_ge:                         \
    case Instruction::i32_clz:                        \
    case Instruction::i32_ctz:                        \
    case Instruction::i32_popcnt:                     \
    case Instruction::i32_add:                        \
    case Instruction::i32_sub:                        \
    case Instruction::i32_mul:                        \
    case Instruction::i32_div_s:                      \
    case Instruction::i32_div_u:                      \
    case Instruction::i32_rem_s:                      \
    case Instruction::i32_rem_u:                      \
    case Instruction::i32_and:                        \
    case Instruction::i32_or:                         \
    case Instruction::i32_xor:                        \
    case Instruction::i32_shl:                        \
    case Instruction::i32_shr_s:                      \
    case Instruction::i32_shr_u:                      \
    case Instruction::i32_rotl:                       \
    case Instruction::i32_rotr:                       \
    case Instruction::i64_clz:                        \
    case Instruction::i64_ctz:                        \
    case Instruction::i64_popcnt:                     \
    case Instruction::i64_add:                        \
    case Instruction::i64_sub:                        \
    case Instruction::i64_mul:                        \
    case Instruction::i64_div_s:                      \
    case Instruction::i64_div_u:                      \
    case Instruction::i64_rem_s:                      \
    case Instruction::i64_rem_u:                      \
    case Instruction::i64_and:                        \
    case Instruction::i64_or:                         \
    case Instruction::i64_xor:                        \
    case Instruction::i64_shl:                        \
    case Instruction::i64_shr_s:                      \
    case Instruction::i64_shr_u:                      \
    case Instruction::i64_rotl:                       \
    case Instruction::i64_rotr:                       \
    case Instruction::f32_abs:                        \
    case Instruction::f32_neg:                        \
    case Instruction::f32_ceil:                       \
    case Instruction::f32_floor:                      \
    case Instruction::f32_trunc:                      \
    case Instruction::f32_nearest:                    \
    case Instruction::f32_sqrt:                       \
    case Instruction::f32_add:                        \
    case Instruction::f32_sub:                        \
    case Instruction::f32_mul:                        \
    case Instruction::f32_div:                        \
    case Instruction::f32_min:                        \
    case Instruction::f32_max:                        \
    case Instruction::f32_copysign:                   \
    case Instruction::f64_abs:                        \
    case Instruction::f64_neg:                        \
    case Instruction::f64_ceil:                       \
    case Instruction::f64_floor:                      \
    case Instruction::f64_trunc:                      \
    case Instruction::f64_nearest:                    \
    case Instruction::f64_sqrt:                       \
    case Instruction::f64_add:                        \
    case Instruction::f64_sub:                        \
    case Instruction::f64_mul:                        \
    case Instruction::f64_div:                        \
    case Instruction::f64_min:                        \
    case Instruction::f64_max:                        \
    case Instruction::f64_copysign:                   \
    case Instruction::i32_wrap_i64:                   \
    case Instruction::i32_trunc_f32_s:                \
    case Instruction::i32_trunc_f32_u:                \
    case Instruction::i32_trunc_f64_s:                \
    case Instruction::i32_trunc_f64_u:                \
    case Instruction::i64_extend_i32_s:               \
    case Instruction::i64_extend_i32_u:               \
    case Instruction::i64_trunc_f32_s:                \
    case Instruction::i64_trunc_f32_u:                \
    case Instruction::i64_trunc_f64_s:                \
    case Instruction::i64_trunc_f64_u:                \
    case Instruction::f32_convert_i32_s:              \
    case Instruction::f32_convert_i32_u:              \
    case Instruction::f32_convert_i64_s:              \
    case Instruction::f32_convert_i64_u:              \
    case Instruction::f32_demote_f64:                 \
    case Instruction::f64_convert_i32_s:              \
    case Instruction::f64_convert_i32_u:              \
    case Instruction::f64_convert_i64_s:              \
    case Instruction::f64_convert_i64_u:              \
    case Instruction::f64_promote_f32:                \
    case Instruction::i32_reinterpret_f32:            \
    case Instruction::i64_reinterpret_f64:            \
    case Instruction::f32_reinterpret_i32:            \
    case Instruction::f64_reinterpret_i64:            \
    case Instruction::i32_extend8_s:                  \
    case Instruction::i32_extend16_s:                 \
    case Instruction::i64_extend8_s:                  \
    case Instruction::i64_extend16_s:                 \
    case Instruction::i64_extend32_s

#define INDEX_ARG_CASE                         \
    Instruction::br : case Instruction::br_if: \
    case Instruction::call:                    \
    case Instruction::ref_func:                \
    case Instruction::local_get:               \
    case Instruction::local_set:               \
    case Instruction::local_tee:               \
    case Instruction::global_get:              \
    case Instruction::global_set:              \
    case Instruction::table_set:               \
    case Instruction::table_get:               \
    case Instruction::elem_drop:               \
    case Instruction::table_grow:              \
    case Instruction::table_size:              \
    case Instruction::table_fill:              \
    case Instruction::data_drop

#define MEM_ARG_CASE                                    \
    Instruction::i32_load : case Instruction::i64_load: \
    case Instruction::f32_load:                         \
    case Instruction::f64_load:                         \
    case Instruction::i32_load8_s:                      \
    case Instruction::i32_load8_u:                      \
    case Instruction::i32_load16_s:                     \
    case Instruction::i32_load16_u:                     \
    case Instruction::i64_load8_s:                      \
    case Instruction::i64_load8_u:                      \
    case Instruction::i64_load16_s:                     \
    case Instruction::i64_load16_u:                     \
    case Instruction::i64_load32_s:                     \
    case Instruction::i64_load32_u:                     \
    case Instruction::i32_store:                        \
    case Instruction::i64_store:                        \
    case Instruction::f32_store:                        \
    case Instruction::f64_store:                        \
    case Instruction::i32_store8:                       \
    case Instruction::i32_store16:                      \
    case Instruction::i64_store8:                       \
    case Instruction::i64_store16:                      \
    case Instruction::i64_store32

#define TWO_INDEX_ARG_CASE                                     \
    Instruction::call_indirect : case Instruction::table_init: \
    case Instruction::table_copy

#define BLOCK_ARG_CASE \
    Instruction::block : case Instruction::loop

#define MEMORY_RESERVED_CASE                                  \
    Instruction::memory_size : case Instruction::memory_grow: \
    case Instruction::memory_fill:                            \
    case Instruction::memory_copy:                            \
    case Instruction::memory_init

#define LITERAL_ARG_CASE                                  \
    Instruction::i32_const : case Instruction::i64_const: \
    case Instruction::f32_const:                          \
    case Instruction::f64_const

    inline result<void> render_instr(const Op& op, binary::writer& w) {
        auto val = std::uint32_t(op.instr);
        if (val > 0xff) {
            return write_byte(w, byte(val >> 8)) &
                   [&] { return render_uint(w, std::uint32_t(val & 0xff)); };
        }
        else {
            return write_byte(w, byte(val));
        }
        switch (op.instr) {
            default:
                return unexpect(Error::unexpected_instruction);
            case NO_ARG_CASE:
                return {};
            // block
            case BLOCK_ARG_CASE:
                return op.as<Block>().and_then([&](auto b) { return render_block(*b, w); });
            // block or block else block
            case Instruction::if_:
                return op.as<If>().and_then([&](auto b) { return render_if(*b, w); });
            case INDEX_ARG_CASE:
                return op.as<Index>().and_then([&](auto b) { return render_uint<std::uint32_t>(w, *b); });
            case Instruction::br_table: {
                return op.as<LabelTable>().and_then([&](auto b) { return render_label_table(*b, w); });
            }
            case TWO_INDEX_ARG_CASE: {
                return op.as<Index2>().and_then([&](auto b) { return render_2uint(w, b->index1, b->index2); });
            }
            case MEM_ARG_CASE: {
                return op.as<Mem>().and_then([&](auto b) { return render_2uint(w, b->align, b->offset); });
            }
            case MEMORY_RESERVED_CASE: {
                return render_memory_xxx_instruction_arg(op, w);
            }
            // reftype
            case Instruction::ref_null:
                return op.as<type::Type>().and_then([&](auto b) { return type::render_reftype(w, *b); });
            // vec<valtype>
            case Instruction::select_t:
                return op.as<type::ResultType>().and_then([&](auto b) { return type::render_result_type(w, *b); });

            case LITERAL_ARG_CASE: {
                return render_literal(op, w);
            }
        }
    }

    inline result<Op> parse_instr(binary::reader& r) {
        auto t = read_byte(r).and_then([&](byte t) -> result<Instruction> {
            auto instr = Instruction(t);
            if (instr == Instruction::need_more_1 || instr == Instruction::need_more_2) {
                auto val = parse_uint<std::uint32_t>(r);
                if (!val) {
                    return val.transform([](auto) { return Instruction{}; });
                }
                if (*val > 0xff) {
                    return unexpect(Error::large_int);
                }
                return Instruction((std::uint32_t(t) << 8) | *val);
            }
            return instr;
        });
        if (!t) {
            return t & empty_value<Op>();
        }
        auto& instr = *t;
        switch (instr) {
            default:
                return unexpect(Error::unexpected_instruction);
            case NO_ARG_CASE:
                return Op{instr};
            // block
            case BLOCK_ARG_CASE:
                return parse_block(r).transform(convert_to_Op(instr));
            // block or block else block
            case Instruction::if_:
                return parse_if(r).transform(convert_to_Op(instr));
            // index
            case INDEX_ARG_CASE:
                return parse_uint<std::uint32_t>(r).transform(convert_to_Op(instr));
            // vec<label> label
            case Instruction::br_table: {
                return parse_label_table(r).transform(convert_to_Op(instr));
            }
            // xx_index xx_index
            case TWO_INDEX_ARG_CASE: {
                Index2 indir;
                return parse_2uint(r, indir.index1, indir.index2).transform([&] { return Op{instr, indir}; });
            }
            // memarg
            case MEM_ARG_CASE: {
                Mem mem;
                return parse_2uint(r, mem.align, mem.offset).transform([&] { return Op{instr, mem}; });
            }
            case MEMORY_RESERVED_CASE: {
                return parse_memory_xxx_instruction_arg(instr, r);
            }
            // reftype
            case Instruction::ref_null:
                return type::parse_reftype(r).transform(convert_to_Op(instr));
            // vec<valtype>
            case Instruction::select_t:
                return type::parse_result_type(r).transform(convert_to_Op(instr));
            case LITERAL_ARG_CASE:
                return parse_literal(instr, r).transform(convert_to_Op(instr));
        }
    }

#undef NO_ARG_CASE
#undef INDEX_ARG_CASE
#undef MEM_ARG_CASE
#undef TWO_INDEX_ARG_CASE
#undef BLOCK_ARG_CASE
#undef MEMORY_RESERVED_CASE
#undef LITERAL_ARG_CASE
}  // namespace utils::wasm::code
