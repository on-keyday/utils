/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// ipret - minilang byte code interpreter
#pragma once
#include <type_traits>
#include <cstdint>

namespace minlc {
    namespace ipret {
        using byte = unsigned char;
        struct Vec {
            byte* area;
            size_t ptr;
            size_t limit;
            constexpr size_t remain() const {
                return area && limit > ptr ? limit - ptr : 0;
            }
            constexpr const byte* top() const {
                return area ? area + ptr : nullptr;
            }

            constexpr byte* wtop() {
                return area ? area + ptr : nullptr;
            }

            constexpr void fwd(size_t len) {
                ptr += len;
                if (ptr > limit) {
                    ptr = limit;
                }
            }

            constexpr void bwd(size_t len) {
                if (ptr < len) {
                    ptr = 0;
                    return;
                }
                ptr -= len;
            }
        };

        template <class T>
        constexpr bool load_i(const Vec& input, T& t) {
            if (input.remain() < sizeof(T)) {
                return false;
            }
            const auto top = input.top();
            T value = T();
            for (auto i = 0; i < sizeof(T); i++) {
                value |= T(byte(top[i])) << (8 * (sizeof(T) - 1 - i));
            }
            return true;
        }

        template <class T>
        constexpr bool store_i(Vec& output, T t) {
            if (output.remain() < sizeof(T)) {
                return false;
            }
            auto top = output.wtop();
            for (auto i = 0; i < sizeof(T); i++) {
                top[i] = byte((std::make_unsigned_t<T>(t) >> (8 * (sizeof(T) - 1 - i))) & 0xff);
            }
            return true;
        }

        constexpr bool load_b(const Vec& input, byte* data, size_t len) {
            if (input.remain() < len) {
                return false;
            }
            const auto top = input.top();
            for (auto i = 0; i < len; i++) {
                data[i] = top[i];
            }
            return true;
        }

        constexpr bool store_b(Vec& output, const byte* data, size_t len) {
            if (output.remain() < len) {
                return false;
            }
            auto top = output.wtop();
            for (auto i = 0; i < len; i++) {
                top[i] = data[i];
            }
            return true;
        }

        constexpr byte get_len(byte param) {
            return 0x1 << ((0xC0 & param) >> 6);
        }

        template <class T>
        constexpr bool load_l(const Vec& input, byte len, T& t) {
            if (len == 1) {
                byte value;
                if (!load_i(input, value)) {
                    return false;
                }
                t = value;
                return true;
            }
            else if (len == 2) {
                std::uint16_t value;
                if (!load_i(input, value)) {
                    return false;
                }
                t = value;
                return true;
            }
            else if (len == 4) {
                std::uint32_t value;
                if (!load_i(input, value)) {
                    return false;
                }
                t = value;
                return true;
            }
            else if (len == 8) {
                std::uint64_t value;
                if (!load_i(input, value)) {
                    return false;
                }
                t = value;
                return true;
            }
            return false;
        }

        template <class T>
        constexpr bool store_l(Vec& output, byte len, T t) {
            if (len == 1) {
                byte value = byte(t);
                if (!store_i(output, value)) {
                    return false;
                }
                return true;
            }
            else if (len == 2) {
                std::uint16_t value = std::uint16_t(t);
                if (!store_i(output, value)) {
                    return false;
                }
                return true;
            }
            else if (len == 4) {
                std::uint32_t value = std::uint32_t(t);
                if (!load_i(output, value)) {
                    return false;
                }
                return true;
            }
            else if (len == 8) {
                std::uint64_t value = std::uint64_t(t);
                if (!load_i(output, value)) {
                    return false;
                }
                return true;
            }
            return false;
        }

        template <class T>
        constexpr byte len_by_num(T num) {
            auto cmp = std::make_unsigned_t<T>(num);
            if (cmp <= 0xff) {
                return 1;
            }
            if (cmp <= 0xffff) {
                return 2;
            }
            if (cmp <= 0xffffffff) {
                return 4;
            }
            return 8;
        }

        constexpr byte len_to_mask(byte len) {
            if (len == 1) {
                return 0x0;
            }
            if (len == 2) {
                return 0x40;
            }
            if (len == 4) {
                return 0x80;
            }
            if (len == 8) {
                return 0xC0;
            }
            int* ptr = nullptr;
            *ptr = 0;  // terminate
            return -1;
        }

        enum instruction {
            // NOP - no operation
            // NOP
            ist_nop,
            // LOAD - load immediate
            // LOAD <length> <register> <value>
            ist_load,
            // PUSH - push register value to stack
            // PUSH <length> <register> <value>
            ist_push,
            // POP - pop stack value to register
            // POP <length> <register> <value>
            ist_pop,
            // CLFLAG - clear flag
            // CLFLAG
            ist_clflag,
            // HLT - halt operation
            // HLT
            ist_hlt,
            // JMP - jmp pc
            // JMP <register>
            ist_jmp,
            // JT - jmp if true
            // JT <register>
            ist_jt,
            // JF - jmp if false
            // JF <register>
            ist_jf,
            // SYSCALL - system call
            // SYSCALL
            ist_syscall,
            // MLOAD - load from memory
            // MLOAD <length> <address_register> <dst_register>
            ist_mload,
            // MSTORE - store to memory
            // MSTORE <length> <address_register> <src_register>
            ist_mstore,
            // STKPTR - get stack pointer
            // STKPTR
            ist_stkptr,
            // STKROOT - get stack root pointer
            // STKROOT
            ist_stkroot,
            // ISTPC - get current pc
            // ISTPC
            ist_istpc,
            // ISTPTR - get instruction pointer
            // ISTPTR
            ist_istptr,
            // ISTROOT - get instruction root pointer
            // ISTROOT
            ist_istroot,
            // TEST - test register value
            // TEST <register>
            ist_test,
            // TRSF - transfer value between registers
            // TRSF <src_register> <dst_register>
            ist_trsf,
            // binary op
            // FARTOP <op_code> <left_src_register> <right_src_register> <dst_register>
            //            // 0  1  2  3
            ist_4artop,   // +  -  *  /
            ist_modbit,   // %  &  |  ^
            ist_shifteq,  // << >> == !=
            ist_cmplogi,  // <  <= || &&
            ist_bitnot,   // ~
        };

        struct Register {
            size_t store[16];
        };

        enum env_flag {
            none,
            f_illegal = 0x1,
            f_length = 0x2,
            f_unspec = 0x4,
            f_eoi = 0x8,
            f_true = 0x10,
            f_div = 0x20,
        };

        struct Env;

        struct Syscall {
            void (*call)(Env& env, void* dat);
            void* dat;
        };

        struct Env {
            Register reg;
            Vec istr;
            Vec stack;
            byte flag;
            Syscall* syscall;
            size_t numsyscall;
            bool (*mprot)(Env& env, size_t& p, size_t len);
        };

        void interpret(Env& env);
    }  // namespace ipret
}  // namespace minlc
