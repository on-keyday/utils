/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <binary/writer.h>

namespace futils::jit::x64 {
    enum Register {
        RAX,
        RCX,
        RDX,
        RBX,
        RSP,
        RBP,
        RSI,
        RDI,
        R8,
        EXTENDED_REGISTER = R8,
        R9,
        R10,
        R11,
        R12,
        R13,
        R14,
        R15,
    };

    constexpr byte make_rex_prefix(bool w, bool r, bool x, bool b) {
        return 0x40 | (byte(w) << 3) | (byte(r) << 2) | (byte(x) << 1) | byte(b);
    }
    constexpr byte make_mod_rm(byte mod, byte reg, byte rm) {
        return ((mod & 0x3) << 6) | ((reg & 0x7) << 3) | (rm & 0x7);
    }

    enum OpCode {
        MOV_REG_REG = 0x89,
        MOV_MEM_REG = 0x8B,
        PUSH_REG = 0x50,
        POP_REG = 0x58,
        NOP = 0x90,
    };

    enum Mod {
        MOD_REG = 0b11,
        MOD_MEM = 0b00,
    };

    constexpr bool emit_mov_reg_reg_or_mem(binary::writer& w, Register from, Register to, Mod mod) {
        auto rex_prefix = make_rex_prefix(1, from >= EXTENDED_REGISTER, 0, to >= EXTENDED_REGISTER);
        return w.write(rex_prefix, 1) &&
               w.write(0x89, 1) &&
               w.write(make_mod_rm(mod, from, to), 1);
    }

    constexpr bool emit_mov_reg_reg(binary::writer& w, Register from, Register to) {
        return emit_mov_reg_reg_or_mem(w, from, to, MOD_REG);
    }

    constexpr bool emit_mov_reg_mem(binary::writer& w, Register from, Register to) {
        return emit_mov_reg_reg_or_mem(w, from, to, MOD_MEM);
    }

    constexpr bool emit_mov_mem_reg(binary::writer& w, Register from, Register to) {
        auto rex_prefix = make_rex_prefix(1, to >= EXTENDED_REGISTER, 0, from >= EXTENDED_REGISTER);
        return w.write(rex_prefix, 1) &&
               w.write(MOV_MEM_REG, 1) &&
               w.write(make_mod_rm(MOD_MEM, from, to), 1);
    }

    constexpr bool emit_push_reg(binary::writer& w, Register reg) {
        auto rex_prefix = make_rex_prefix(1, reg >= EXTENDED_REGISTER, 0, 0);
        return w.write(rex_prefix, 1) &&
               w.write(PUSH_REG + (reg & 0x7), 1);
    }

    constexpr bool emit_pop_reg(binary::writer& w, Register reg) {
        auto rex_prefix = make_rex_prefix(1, reg >= EXTENDED_REGISTER, 0, 0);
        return w.write(rex_prefix, 1) &&
               w.write(POP_REG + (reg & 0x7), 1);
    }

    constexpr bool emit_nop(binary::writer& w) {
        return w.write(NOP, 1);
    }

}  // namespace futils::jit::x64
