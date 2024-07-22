/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "x64.h"
#include "relocation.h"
#include <platform/detect.h>
#include <bit>

namespace futils::jit::x64::coro {
    constexpr auto rbp_saved = futils::jit::x64::Register::R9;
    constexpr auto rsp_saved = futils::jit::x64::Register::R10;

    constexpr auto next_instruction_pointer_offset = 8 * 1;
    constexpr auto next_stack_rbp_offset = 8 * 2;
    constexpr auto next_stack_rsp_offset = 8 * 3;
    constexpr auto original_rbp_offset = 8 * 3;
    constexpr auto original_rsp_offset = 8 * 5;
    constexpr auto coroutine_epilogue_offset = 8 * 6;

    struct StackBottom {
        std::uint64_t coroutine_epilogue_address;
        std::uint64_t original_rsp;
        std::uint64_t original_rbp;
        std::uint64_t next_stack_rsp;
        std::uint64_t next_stack_rbp;
        std::uint64_t next_instruction_pointer;
    };

    constexpr void write_coroutine_prologue(futils::binary::writer& w, futils::view::wvec full_memory) {
        // function argument registers are RDI, RSI, RDX, RCX, R8, R9 in Linux
        // function argument registers are RCX, RDX, R8, R9 in Windows
        // this function is int (*)(std::uintptr_t syscall_result)
        // stack bottom layout (from bottom):
        // 0. next instruction pointer like coroutine
        // 1. next stack rbp like coroutine
        // 2. next stack rsp like coroutine
        // 3. original (system) rbp
        // 4. original (system) rsp
        // 5. aligning to 16 bytes
        // at entry, here is the stack bottom

        // save rbp and rsp
        futils::jit::x64::emit_mov_reg_reg(w, futils::jit::x64::Register::RSP, rsp_saved);  // SAVE RSP
        futils::jit::x64::emit_mov_reg_reg(w, futils::jit::x64::Register::RBP, rbp_saved);  // SAVE RBP
        // change stack pointer to point to the bottom of the stack
        auto stack_bottom = std::bit_cast<std::uintptr_t>(full_memory.data() + full_memory.size());
        auto next_instruction_pointer = stack_bottom - next_instruction_pointer_offset;
        auto next_stack_rbp = stack_bottom - next_stack_rbp_offset;
        auto next_stack_rsp = stack_bottom - next_stack_rsp_offset;
        auto rbp_bottom = stack_bottom - original_rbp_offset;  // for coroutine instruction pointer, rbp, rsp
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RSP, rbp_bottom);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RBP, rbp_bottom);
        futils::jit::x64::emit_push_reg(w, rsp_saved);  // PUSH RSP
        futils::jit::x64::emit_push_reg(w, rbp_saved);  // PUSH RBP
        auto initial_stack_pointer = stack_bottom - coroutine_epilogue_offset;
        futils::binary::writer mem{full_memory};
        mem.reset(full_memory.size() - next_stack_rsp_offset);
        futils::binary::write_num(mem, initial_stack_pointer, false);  // writing next rsp for first resume
        futils::binary::write_num(mem, initial_stack_pointer, false);  // writing next rbp for first resume
        // now we are going to jump to the suspended point
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RSP, next_stack_rsp);
        futils::jit::x64::emit_mov_reg_mem(w, futils::jit::x64::Register::RSP, futils::jit::x64::Register::RSP);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RBP, next_stack_rbp);
        futils::jit::x64::emit_mov_reg_mem(w, futils::jit::x64::Register::RBP, futils::jit::x64::Register::RBP);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RAX, next_instruction_pointer);
        futils::jit::x64::emit_mov_reg_mem(w, futils::jit::x64::Register::RAX, futils::jit::x64::Register::RAX);
        futils::jit::x64::emit_push_reg(w, futils::jit::x64::Register::RAX);
#ifdef FUTILS_PLATFORM_WINDOWS
        // copy first argument to RAX
        futils::jit::x64::emit_mov_reg_reg(w, futils::jit::x64::Register::RCX, futils::jit::x64::Register::RAX);
#elif defined(FUTILS_PLATFORM_LINUX)
        // copy first argument to RAX
        futils::jit::x64::emit_mov_reg_reg(w, futils::jit::x64::Register::RDI, futils::jit::x64::Register::RAX);
#endif
        futils::jit::x64::emit_ret(w);  // jump to the suspended point
    }

    constexpr void write_coroutine_epilogue(futils::binary::writer& w, futils::view::wvec full_memory) {
        auto initial_stack_pointer = std::bit_cast<std::uintptr_t>(full_memory.data() + full_memory.size() - 8 * 5);
        auto rbp_bottom = std::bit_cast<std::uintptr_t>(full_memory.data() + full_memory.size()) - 8 * 3;
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RSP, initial_stack_pointer);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RBP, rbp_bottom);
        futils::jit::x64::emit_pop_reg(w, rbp_saved);  // POP RBP
        futils::jit::x64::emit_pop_reg(w, rsp_saved);  // POP RSP
        futils::jit::x64::emit_mov_reg_reg(w, rbp_saved, futils::jit::x64::Register::RBP);
        futils::jit::x64::emit_mov_reg_reg(w, rsp_saved, futils::jit::x64::Register::RSP);
        futils::jit::x64::emit_ret(w);
    }

    constexpr void write_save_instruction_pointer(futils::binary::writer& w, futils::view::wvec full_memory, std::uint64_t instruction_pointer) {
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::R9, instruction_pointer);
        auto next_instruction_pointer_saved = std::bit_cast<std::uintptr_t>(full_memory.data() + full_memory.size() - next_instruction_pointer_offset);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::R8, next_instruction_pointer_saved);
        futils::jit::x64::emit_mov_mem_reg(w, futils::jit::x64::Register::R9, futils::jit::x64::Register::R8);
    }

    constexpr void write_reset_next_rbp_rsp(futils::binary::writer& w, futils::view::wvec full_memory) {
        auto next_stack_rbp_saved = std::bit_cast<std::uintptr_t>(full_memory.data() + full_memory.size() - next_stack_rbp_offset);
        auto initial_rsp = std::bit_cast<std::uintptr_t>(full_memory.data() + full_memory.size() - original_rsp_offset);
        auto initial_rbp = std::bit_cast<std::uintptr_t>(full_memory.data() + full_memory.size() - original_rbp_offset);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::R8, next_stack_rbp_saved);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::R9, initial_rbp);
        futils::jit::x64::emit_mov_mem_reg(w, futils::jit::x64::Register::R9, futils::jit::x64::Register::R8);
        auto next_stack_rsp_saved = std::bit_cast<std::uintptr_t>(full_memory.data() + full_memory.size() - next_stack_rsp_offset);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::R8, next_stack_rsp_saved);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::R9, initial_rsp);
        futils::jit::x64::emit_mov_mem_reg(w, futils::jit::x64::Register::R9, futils::jit::x64::Register::R8);
    }

    constexpr void write_save_instruction_pointer_rbp_rsp(futils::binary::writer& w, futils::view::wvec full_memory, std::uint64_t instruction_pointer) {
        write_save_instruction_pointer(w, full_memory, instruction_pointer);
        auto next_stack_rbp_saved = std::bit_cast<std::uintptr_t>(full_memory.data() + full_memory.size() - next_stack_rbp_offset);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::R8, next_stack_rbp_saved);
        futils::jit::x64::emit_mov_mem_reg(w, futils::jit::x64::Register::RBP, futils::jit::x64::Register::R8);
        auto next_stack_rsp_saved = std::bit_cast<std::uintptr_t>(full_memory.data() + full_memory.size() - next_stack_rsp_offset);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::R8, next_stack_rsp_saved);
        futils::jit::x64::emit_mov_mem_reg(w, futils::jit::x64::Register::RSP, futils::jit::x64::Register::R8);
    }

    constexpr void write_initial_next_instruction_pointer(futils::view::wvec full_memory, std::uint64_t next_instruction_pointer) {
        futils::binary::writer mem{full_memory};
        mem.reset(full_memory.size() - next_instruction_pointer_offset);
        futils::binary::write_num(mem, next_instruction_pointer, false);
    }

    constexpr void write_epilogue_instruction_pointer(futils::view::wvec full_memory, std::uint64_t instruction_pointer) {
        futils::binary::writer mem{full_memory};
        mem.reset(full_memory.size() - coroutine_epilogue_offset);
        futils::binary::write_num(mem, instruction_pointer, false);
    }

    constexpr void relocate(futils::view::wvec memory, const RelocationEntry& relocation, futils::view::wvec stack_memory) {
        futils::binary::writer mem{memory};
        auto origin = memory.data();
        switch (relocation.type) {
            case SAVE_RIP_RBP_RSP: {
                auto address = std::uint64_t(origin + relocation.address_offset);
                mem.reset(relocation.rewrite_offset);
                write_save_instruction_pointer_rbp_rsp(mem, stack_memory, address);
                assert(mem.offset() == relocation.end_offset);
                break;
            }
            case RESET_RIP: {
                auto address = std::uint64_t(origin + relocation.address_offset);
                mem.reset(relocation.rewrite_offset);
                write_save_instruction_pointer(mem, stack_memory, address);
                assert(mem.offset() == relocation.end_offset);
                break;
            }
            case INITIAL_RIP: {
                write_initial_next_instruction_pointer(stack_memory, std::uint64_t(origin + relocation.address_offset));
                break;
            }
            case EPILOGUE_RIP: {
                write_epilogue_instruction_pointer(stack_memory, std::uint64_t(origin + relocation.address_offset));
                break;
            }
            case SET_FIRST_ARG: {
                auto address = std::uint64_t(origin + relocation.address_offset);
                mem.reset(relocation.rewrite_offset);
#ifdef FUTILS_PLATFORM_WINDOWS
                futils::jit::x64::emit_mov_reg_imm(mem, futils::jit::x64::Register::RCX, address);
#else
                futils::jit::x64::emit_mov_reg_imm(mem, futils::jit::x64::Register::RDI, address);
#endif
                assert(mem.offset() == relocation.end_offset);
                break;
            }
            default: {
                break;
            }
        }
    }

    struct CoroutineRelocations {
        RelocationEntry relocations[5];
    };

    inline void* trampoline(void* (*suspend)(void*), void* ctx, void* (*f)(void* (*suspend)(void*), void* ctx), StackBottom* bottom) {
        return f(suspend, ctx);
    }

    template <class A, class B>
        requires(std::is_integral_v<A> || std::is_pointer_v<A>) && (std::is_integral_v<B> || std::is_pointer_v<B>)
    constexpr CoroutineRelocations write_coroutine(futils::binary::writer& w, futils::view::wvec stack_memory, B (*f)(A (*suspend)(B), A ptr)) {
        write_coroutine_prologue(w, stack_memory);
        // suspend routine
        auto suspend_begin = w.offset();
#ifdef FUTILS_PLATFORM_WINDOWS
        x64::emit_mov_reg_reg(w, x64::Register::RCX, x64::Register::RAX);  // set suspend routine argument to return value
#else
        x64::emit_mov_reg_imm(w, x64::Register::RDI, x64::Register::RAX);
#endif
        auto save_begin = w.offset();
        write_save_instruction_pointer_rbp_rsp(w, stack_memory, 0);
        auto save_end = w.offset();
        write_coroutine_epilogue(w, stack_memory);
        auto save_instruction_pointer_relocation = RelocationEntry{SAVE_RIP_RBP_RSP, save_begin, save_end, w.offset()};
        x64::emit_ret(w);  // resume routine
        auto initial_instruction_pointer = w.offset();
        auto initial_instruction_pointer_relocation = RelocationEntry{INITIAL_RIP, 0, 0, initial_instruction_pointer};
        auto first_arg_relocation = RelocationEntry{SET_FIRST_ARG, w.offset(), 0, suspend_begin};
#ifdef FUTILS_PLATFORM_WINDOWS
        x64::emit_mov_reg_imm(w, x64::Register::RCX, 0);
        first_arg_relocation.end_offset = w.offset();
        // copy RAX to second argument register
        x64::emit_mov_reg_reg(w, x64::Register::RAX, x64::Register::RDX);
        // copy target function to thrird argument register
        x64::emit_mov_reg_imm(w, x64::Register::R8, std::bit_cast<std::uintptr_t>(f));
        // copy stack bottom to fourth argument register
        x64::emit_mov_reg_imm(w, x64::Register::R9, std::bit_cast<std::uintptr_t>(stack_memory.data() + stack_memory.size() - sizeof(StackBottom)));
#else
        x64::emit_mov_reg_imm(w, x64::Register::RDI, 0);
        first_arg_relocation.end_offset = w.offset();
        // copy RAX to second argument register
        x64::emit_mov_reg_reg(w, x64::Register::RAX, x64::Register::RSI);
        // copy target function to thrird argument register
        x64::emit_mov_reg_imm(w, x64::Register::RDX, std::bit_cast<std::uintptr_t>(f));
        // copy stack bottom to fourth argument register
        x64::emit_mov_reg_imm(w, x64::Register::RCX, std::bit_cast<std::uintptr_t>(stack_memory.data() + stack_memory.size() - sizeof(StackBottom)));
#endif
        auto t = trampoline;
        x64::emit_mov_reg_imm(w, x64::Register::RAX, std::bit_cast<std::uintptr_t>(t));
        x64::emit_call_reg(w, x64::Register::RAX);
        auto epilogue_relocation = RelocationEntry{EPILOGUE_RIP, 0, 0, w.offset()};
        auto reset_instruction_pointer_relocation = RelocationEntry{RESET_RIP, w.offset(), 0, initial_instruction_pointer};
        write_save_instruction_pointer(w, stack_memory, 0);
        reset_instruction_pointer_relocation.end_offset = w.offset();
        write_reset_next_rbp_rsp(w, stack_memory);
        write_coroutine_epilogue(w, stack_memory);
        return CoroutineRelocations{
            save_instruction_pointer_relocation,
            reset_instruction_pointer_relocation,
            initial_instruction_pointer_relocation,
            first_arg_relocation,
            epilogue_relocation,
        };
    }

}  // namespace futils::jit::x64::coro
