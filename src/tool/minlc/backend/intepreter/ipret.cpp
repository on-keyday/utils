/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "ipret.h"

namespace minlc {
    namespace ipret {
        bool get_len_and_regnum(Env& env, byte& len, byte& regnum) {
            byte param = 0;
            if (!load_i(env.istr, param)) {
                env.flag = f_illegal | f_length;
                return false;  // illegal instruction
            }
            env.istr.fwd(1);
            len = get_len(param);
            regnum = param & 0x2f;
            if (regnum >= 16) {
                env.flag = f_illegal | f_unspec;
                return false;  // illegal instruction
            }
            return true;
        }

        void load(Env& env) {
            byte len, regnum;
            if (!get_len_and_regnum(env, len, regnum)) {
                return;
            }
            if (!load_l(env.istr, len, env.reg.store[regnum])) {
                env.flag = f_illegal | f_length;
                return;
            }
            env.istr.fwd(len);
            // instruction done
        }

        void push(Env& env) {
            byte len, regnum;
            if (!get_len_and_regnum(env, len, regnum)) {
                return;
            }
            size_t to_store = env.reg.store[regnum];
            if (!store_l(env.stack, len, to_store)) {
                env.flag = f_illegal | f_length;
                return;
            }
            env.stack.fwd(len);
        }

        void pop(Env& env) {
            byte len, regnum;
            if (!get_len_and_regnum(env, len, regnum)) {
                return;
            }
            env.stack.bwd(len);
            if (!load_l(env.stack, len, env.reg.store[regnum])) {
                env.flag = f_illegal | f_length;
                return;
            }
        }

        void jmp(Env& env) {
            byte _, regnum;
            if (!get_len_and_regnum(env, _, regnum)) {
                return;
            }
            env.istr.ptr = env.reg.store[regnum];
        }

        void jt(Env& env) {
            byte _, regnum;
            if (!get_len_and_regnum(env, _, regnum)) {
                return;
            }
            if (env.flag & f_true) {
                env.istr.ptr = env.reg.store[regnum];
            }
        }

        void jf(Env& env) {
            byte _, regnum;
            if (!get_len_and_regnum(env, _, regnum)) {
                return;
            }
            if (!(env.flag & f_true)) {
                env.istr.ptr = env.reg.store[regnum];
            }
        }

        void syscall(Env& env) {
            const auto num = env.reg.store[0];
            if (!env.syscall || num >= env.numsyscall) {
                env.flag |= f_illegal | f_unspec;
                return;
            }
            auto& syscall = env.syscall[num];
            if (!syscall.call) {
                env.flag |= f_illegal | f_unspec;
                return;
            }
            syscall.call(env, syscall.dat);
        }

        void mload(Env& env) {
            byte len, ptr_reg, _, load_reg;
            if (!get_len_and_regnum(env, len, ptr_reg) ||
                !get_len_and_regnum(env, _, load_reg)) {
                return;
            }
            size_t ptr = env.reg.store[ptr_reg];
            if (!env.mprot ||
                !env.mprot(env, ptr, len)) {
                env.flag |= f_illegal | f_unspec;
                return;
            }
            auto raw_ptr = reinterpret_cast<byte*>(ptr);
            Vec vec;
            vec.area = raw_ptr;
            vec.ptr = 0;
            vec.limit = len;
            if (!load_l(vec, len, env.reg.store[load_reg])) {
                env.flag |= f_illegal | f_length;
                return;
            }
        }

        void mstore(Env& env) {
            byte len, ptr_reg, _, store_reg;
            if (!get_len_and_regnum(env, len, ptr_reg) ||
                !get_len_and_regnum(env, _, store_reg)) {
                return;
            }
            size_t ptr = env.reg.store[ptr_reg];
            if (!env.mprot ||
                !env.mprot(env, ptr, len)) {
                env.flag |= f_illegal | f_unspec;
                return;
            }
            auto raw_ptr = reinterpret_cast<byte*>(ptr);
            Vec vec;
            vec.area = raw_ptr;
            vec.ptr = 0;
            vec.limit = len;
            if (!store_l(vec, len, env.reg.store[store_reg])) {
                env.flag |= f_illegal | f_length;
                return;
            }
        }

        void test(Env& env) {
            byte _, regnum;
            if (!get_len_and_regnum(env, _, regnum)) {
                return;
            }
            if (env.reg.store[regnum]) {
                env.flag |= f_true;
            }
            else {
                env.flag &= ~f_true;
            }
        }

        void fourop_cb(Env& env, auto&& cb) {
            byte op, first, _, second, dst;
            if (!get_len_and_regnum(env, op, first) ||
                !get_len_and_regnum(env, _, second) ||
                !get_len_and_regnum(env, _, dst)) {
                return;
            }
            auto f_v = env.reg.store[first];
            auto s_v = env.reg.store[second];
            auto& t_v = env.reg.store[dst];
            op >>= 1;
            cb(env, op, f_v, s_v, t_v);
        }

        void fourartop(Env& env, byte op, size_t f_v, size_t s_v, size_t& t_v) {
            if (op == 0) {
                t_v = f_v + s_v;
            }
            else if (op == 1) {
                t_v = f_v - s_v;
            }
            else if (op == 2) {
                t_v = f_v * s_v;
            }
            else {
                if (s_v == 0) {
                    env.flag |= f_illegal | f_div;
                    return;
                }
                t_v = f_v / s_v;
            }
        }

        void modbit(Env& env, byte op, size_t f_v, size_t s_v, size_t& t_v) {
            if (op == 0) {
                if (s_v == 0) {
                    env.flag |= f_illegal | f_div;
                    return;
                }
                t_v = f_v % s_v;
            }
            else if (op == 1) {
                t_v = f_v & s_v;
            }
            else if (op == 2) {
                t_v = f_v | s_v;
            }
            else {
                t_v = f_v ^ s_v;
            }
        }

        void shifteq(Env& env, byte op, size_t f_v, size_t s_v, size_t& t_v) {
            if (op == 0) {
                t_v = f_v >> s_v;
            }
            else if (op == 1) {
                t_v = f_v << s_v;
            }
            else if (op == 2) {
                t_v = f_v == s_v;
            }
            else {
                t_v = f_v != s_v;
            }
        }

        void cmplogi(Env& env, byte op, size_t f_v, size_t s_v, size_t& t_v) {
            if (op == 0) {
                t_v = f_v < s_v;
            }
            else if (op == 1) {
                t_v = f_v <= s_v;
            }
            else if (op == 2) {
                t_v = f_v && s_v;
            }
            else {
                t_v = f_v || s_v;
            }
        }

        void bitnot(Env& env) {
            byte len, from_reg, _, to_reg;
            if (!get_len_and_regnum(env, len, from_reg) ||
                !get_len_and_regnum(env, _, to_reg)) {
                return;
            }
            env.reg.store[to_reg] = ~env.reg.store[from_reg];
        }

        void trsf(Env& env) {
            byte from_reg, _, to_reg;
            if (!get_len_and_regnum(env, _, from_reg) ||
                !get_len_and_regnum(env, _, to_reg)) {
                return;
            }
            env.reg.store[to_reg] = env.reg.store[from_reg];
        }

        void interpret(Env& env) {
            byte ist = 0;
            if (!load_i(env.istr, ist)) {
                env.flag |= f_eoi;
                return;  // end of instruction
            }
            env.istr.fwd(1);
            if (ist == ist_nop) {
                return;  // nothing to do
            }
            if (ist == ist_hlt) {
                env.flag |= f_eoi;
                return;
            }
            if (ist == ist_clflag) {
                env.flag = 0;
                return;
            }
            if (ist == ist_stkptr) {
                if (env.stack.area) {
                    env.reg.store[0] = std::uintptr_t(env.stack.area + env.stack.ptr);
                }
                else {
                    env.reg.store[0] = 0;
                }
                return;
            }
            if (ist == ist_stkroot) {
                env.reg.store[0] = std::uintptr_t(env.stack.area);
                return;
            }
            if (ist == ist_istpc) {
                env.reg.store[0] = env.istr.ptr;
                return;
            }
            if (ist == ist_istptr) {
                if (env.istr.area) {
                    env.reg.store[0] = std::uintptr_t(env.istr.area + env.istr.ptr);
                }
                else {
                    env.reg.store[0] = 0;
                }
                return;
            }
            if (ist == ist_istroot) {
                env.reg.store[0] = std::uintptr_t(env.istr.area);
                return;
            }
            if (ist == ist_load) {
                load(env);
                return;
            }
            if (ist == ist_push) {
                push(env);
                return;
            }
            if (ist == ist_pop) {
                pop(env);
                return;
            }
            if (ist == ist_jmp) {
                jmp(env);
                return;
            }
            if (ist == ist_jt) {
                jt(env);
                return;
            }
            if (ist == ist_jf) {
                jf(env);
                return;
            }
            if (ist == ist_syscall) {
                syscall(env);
                return;
            }
            if (ist == ist_mload) {
                mload(env);
                return;
            }
            if (ist == ist_mstore) {
                mstore(env);
                return;
            }
            if (ist == ist_test) {
                test(env);
                return;
            }
            if (ist == ist_4artop) {
                fourop_cb(env, fourartop);
                return;
            }
            if (ist == ist_modbit) {
                fourop_cb(env, modbit);
                return;
            }
            if (ist == ist_shifteq) {
                fourop_cb(env, shifteq);
                return;
            }
            if (ist == ist_cmplogi) {
                fourop_cb(env, cmplogi);
                return;
            }
            if (ist == ist_bitnot) {
                bitnot(env);
                return;
            }
            if (ist == ist_trsf) {
                trsf(env);
                return;
            }
            env.flag |= f_illegal | f_unspec;
            return;
        }
    }  // namespace ipret
}  // namespace minlc
