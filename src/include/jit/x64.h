/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <binary/writer.h>
#include <binary/number.h>

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

    constexpr byte make_sib(byte scale, byte index, byte base) {
        return ((scale & 0x3) << 6) | ((index & 0x7) << 3) | (base & 0x7);
    }

    enum OpCode {
        MOV_REG_REG = 0x89,
        MOV_REG_MEM = 0x8B,
        MOV_REG_IMM = 0xB8,
        PUSH_REG = 0x50,
        POP_REG = 0x58,
        NOP = 0x90,
        ADD_REG_IMM = 0x81,
        ADD_REG_REG = 0x01,
        RET = 0xC3,
        CMP_REG_REG = 0x3B,
        JE_REL8 = 0x74,
        JNE_REL8 = 0x75,
    };

    enum Mod {
        MOD_REG = 0b11,
        MOD_MEM = 0b00,
        MOD_MEM_DISP8 = 0b01,
        MOD_MEM_DISP32 = 0b10,
    };

    constexpr bool emit_mov_reg_or_mem_reg(binary::writer& w, Register from, Register to, Mod mod) {
        auto rex_prefix = make_rex_prefix(1, from >= EXTENDED_REGISTER, 0, to >= EXTENDED_REGISTER);
        return w.write(rex_prefix, 1) &&
               w.write(MOV_REG_REG, 1) &&
               w.write(make_mod_rm(mod, from, to), 1);
    }

    constexpr bool emit_mov_reg_reg(binary::writer& w, Register from, Register to) {
        return emit_mov_reg_or_mem_reg(w, from, to, MOD_REG);
    }

    constexpr bool emit_mov_mem_reg(binary::writer& w, Register from, Register to) {
        return emit_mov_reg_or_mem_reg(w, from, to, MOD_MEM);
    }

    constexpr bool emit_mov_reg_mem(binary::writer& w, Register from, Register to) {
        auto rex_prefix = make_rex_prefix(1, to >= EXTENDED_REGISTER, from >= EXTENDED_REGISTER, 0);
        if (from == RSP) {
            // see https://www.wdic.org/w/SCI/SIB%E3%83%90%E3%82%A4%E3%83%88#Base
            // https://gist.github.com/tenpoku1000/24c249e32c512611c079ce87a59a6a52
            return w.write(rex_prefix, 1) &&
                   w.write(MOV_REG_MEM, 1) &&
                   w.write(make_mod_rm(MOD_MEM, to, 0b100 /*SIB*/), 1) &&
                   w.write(make_sib(0, 0b100 /*none*/, RSP), 1);
        }
        if (from == RBP) {
            return w.write(rex_prefix, 1) &&
                   w.write(MOV_REG_MEM, 1) &&
                   w.write(make_mod_rm(MOD_MEM_DISP8, to, RBP), 1) &&
                   w.write(0, 1);  // disp8 = 0
        }
        return w.write(rex_prefix, 1) &&
               w.write(MOV_REG_MEM, 1) &&
               w.write(make_mod_rm(MOD_MEM, to, from), 1);
    }

    constexpr bool emit_mov_reg_imm(binary::writer& w, Register reg, uint64_t imm) {
        auto rex_prefix = make_rex_prefix(1, 0, 0, reg >= EXTENDED_REGISTER);
        return w.write(rex_prefix, 1) &&
               w.write(MOV_REG_IMM + (reg & 0x7), 1) &&
               binary::write_num(w, imm, false);
    }

    constexpr bool emit_push_reg(binary::writer& w, Register reg) {
        auto rex_prefix = make_rex_prefix(1, 0, 0, reg >= EXTENDED_REGISTER);
        return w.write(rex_prefix, 1) &&
               w.write(PUSH_REG + (reg & 0x7), 1);
    }

    constexpr bool emit_pop_reg(binary::writer& w, Register reg) {
        auto rex_prefix = make_rex_prefix(1, 0, 0, reg >= EXTENDED_REGISTER);
        return w.write(rex_prefix, 1) &&
               w.write(POP_REG + (reg & 0x7), 1);
    }

    constexpr bool emit_nop(binary::writer& w) {
        return w.write(NOP, 1);
    }

    constexpr bool emit_add_reg_imm(binary::writer& w, Register reg, uint32_t imm) {
        auto rex_prefix = make_rex_prefix(1, reg >= EXTENDED_REGISTER, 0, 0);
        return w.write(rex_prefix, 1) &&
               w.write(ADD_REG_IMM, 1) &&
               w.write(make_mod_rm(MOD_REG, 0b000, reg), 1) &&
               binary::write_num(w, imm, false);
    }

    constexpr bool emit_add_reg_reg(binary::writer& w, Register from, Register to) {
        auto rex_prefix = make_rex_prefix(1, from >= EXTENDED_REGISTER, 0, to >= EXTENDED_REGISTER);
        return w.write(rex_prefix, 1) &&
               w.write(ADD_REG_REG, 1) &&
               w.write(make_mod_rm(MOD_REG, from, to), 1);
    }

    constexpr bool emit_ret(binary::writer& w) {
        return w.write(RET, 1);
    }

    constexpr bool emit_cmp_reg_reg(binary::writer& w, Register from, Register to) {
        auto rex_prefix = make_rex_prefix(1, from >= EXTENDED_REGISTER, 0, to >= EXTENDED_REGISTER);
        return w.write(rex_prefix, 1) &&
               w.write(CMP_REG_REG, 1) &&
               w.write(make_mod_rm(MOD_REG, from, to), 1);
    }

    constexpr bool emit_je_rel8(binary::writer& w, int8_t offset) {
        return w.write(JE_REL8, 1) &&
               w.write(offset, 1);
    }

    constexpr bool emit_jne_rel8(binary::writer& w, int8_t offset) {
        return w.write(JNE_REL8, 1) &&
               w.write(offset, 1);
    }

}  // namespace futils::jit::x64
