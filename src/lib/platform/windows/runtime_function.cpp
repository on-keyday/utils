/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/windows/dllexport_source.h>
#include <Windows.h>
#include <winnt.h>
#include <cstdint>
#include <view/span.h>
#include <platform/windows/runtime_function.h>
#include <view/iovec.h>
#include <jit/x64_coroutine.h>
#include <array>

namespace futils::platform::windows {

    _IMAGE_DOS_HEADER* get_image_base() {
        return reinterpret_cast<_IMAGE_DOS_HEADER*>(GetModuleHandleW(nullptr));
    }

    IMAGE_OPTIONAL_HEADER* get_optional_header() {
        return reinterpret_cast<IMAGE_OPTIONAL_HEADER*>(reinterpret_cast<std::uintptr_t>(get_image_base()) + get_image_base()->e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER));
    }

    IMAGE_DATA_DIRECTORY get_exception_data_dir() {
        return get_optional_header()->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    }

    futils_DLL_EXPORT view::rspan<RuntimeFunction> STDCALL get_runtime_functions() {
        auto data = get_exception_data_dir();
        auto ptr = data.VirtualAddress + reinterpret_cast<std::uintptr_t>(get_image_base());
        return view::rspan<RuntimeFunction>(reinterpret_cast<RuntimeFunction*>(ptr), data.Size / sizeof(RUNTIME_FUNCTION));
    }

    futils_DLL_EXPORT RealRuntimeFunction STDCALL get_real_runtime_functions(const RuntimeFunction& rf) {
        auto base = reinterpret_cast<std::uintptr_t>(get_image_base());
        return RealRuntimeFunction{
            reinterpret_cast<void*>(rf.BeginAddress + base),
            reinterpret_cast<void*>(rf.EndAddress + base),
            reinterpret_cast<UnwindInfoHeader*>(rf.UnwindInfoAddress + base)};
    }

    futils_DLL_EXPORT void* get_exception_handler_address(const ExceptionHandlerData& eh) {
        auto base = reinterpret_cast<std::uintptr_t>(get_image_base());
        return reinterpret_cast<void*>(eh.begin_address + base);
    }

    struct alignas(16) JITUnwindInfo {
        RUNTIME_FUNCTION rt{};
        UnwindInfoHeader info{};
        UnwindCode codes[2]{};
        std::uint32_t exception_handler = 0;
        byte exception_handler_[13];
        view::wvec stack;
    };

    constexpr auto generate_jmp(std::uintptr_t f) {
        std::array<byte, 13> buf{};
        futils::binary::writer w{buf};
        if (!jit::x64::emit_mov_reg_imm(w, jit::x64::RAX, f) ||
            !jit::x64::emit_jmp_reg(w, jit::x64::RAX)) {
            throw "Failed to generate jmp";
        }
        return buf;
    }

    constexpr auto test_handler = generate_jmp(0);

    struct ScopeInfo {
        std::uint32_t BeginAddress;
        std::uint32_t EndAddress;
        std::uint32_t HandlerAddress;
        std::uint32_t JumpTarget;
    };

    void run_scope_handlers(PEXCEPTION_RECORD ExceptionRecord, ULONG64 EstablisherFrame,
                            PCONTEXT ContextRecord, PDISPATCHER_CONTEXT DispatcherContext) {
        auto base_address = DispatcherContext->ImageBase;
        auto scope_table = (PSCOPE_TABLE)DispatcherContext->HandlerData;
        auto scope_index = DispatcherContext->ScopeIndex;
        auto scope_count = scope_table->Count;
        auto control_pc = DispatcherContext->ControlPc - base_address;

        for (auto i = scope_index; i < scope_count; i++) {
            auto scope = (ScopeInfo*)&scope_table->ScopeRecord[i];
            if (control_pc < scope->BeginAddress || control_pc >= scope->EndAddress) {
                continue;
            }
            if (IS_DISPATCHING(ExceptionRecord->ExceptionFlags)) {
                if (scope->JumpTarget == 0) {
                    continue;
                }
                auto exception_filter_handler = scope->HandlerAddress;
                auto exec = 0;
                if (exception_filter_handler == 1) {
                    exec = EXCEPTION_EXECUTE_HANDLER;
                }
                else {
                    auto filter = (PEXCEPTION_FILTER)(scope->HandlerAddress + base_address);
                    EXCEPTION_POINTERS pointers{
                        .ExceptionRecord = ExceptionRecord,
                        .ContextRecord = ContextRecord,
                    };
                    exec = filter(&pointers, (void*)EstablisherFrame);
                }
                if (exec < 0) {
                    continue;  // TODO(on-keyday): should return?
                }
                else if (exec > 0) {
                    // run destructor?
                    auto handler = (void*)(scope->JumpTarget + base_address);
                    RtlUnwindEx((void*)EstablisherFrame,
                                handler,
                                ExceptionRecord,
                                (PVOID)((ULONG_PTR)ExceptionRecord->ExceptionCode),
                                DispatcherContext->ContextRecord, DispatcherContext->HistoryTable);
                }
            }
            else {
                auto target_pc = DispatcherContext->TargetIp;
                if (!IS_TARGET_UNWIND(ExceptionRecord->ExceptionFlags)) {
                    DWORD target_index = 0;
                    for (target_index = 0; target_index < scope_count; target_index += 1) {
                        if ((target_pc >= scope_table->ScopeRecord[target_index].BeginAddress) &&
                            (target_pc < scope_table->ScopeRecord[target_index].EndAddress) &&
                            (scope_table->ScopeRecord[target_index].JumpTarget == scope_table->ScopeRecord[i].JumpTarget) &&
                            (scope_table->ScopeRecord[target_index].HandlerAddress == scope_table->ScopeRecord[i].HandlerAddress)) {
                            break;
                        }
                    }
                    if (target_index != scope_count) {
                        break;
                    }
                }
                if (scope->JumpTarget != 0) {
                    if ((target_pc == scope->JumpTarget) &&
                        (IS_TARGET_UNWIND(ExceptionRecord->ExceptionFlags))) {
                        break;
                    }
                }
                else {
                    DispatcherContext->ScopeIndex = i + 1;
                    auto terminate =
                        (PTERMINATION_HANDLER)(scope->HandlerAddress + base_address);
                    terminate(true, (void*)EstablisherFrame);
                }
            }
        }
    }

    // see http://doxygen.reactos.org/d8/d2f/unwind_8c.html
    void do_unwind(PEXCEPTION_RECORD ExceptionRecord, ULONG64 EstablisherFrame,
                   PCONTEXT ContextRecord, PDISPATCHER_CONTEXT DispatcherContext) {
        auto base_address = DispatcherContext->ImageBase;
        CONTEXT ctx, unwind_ctx;
        ctx = *ContextRecord;
        unwind_ctx = ctx;
        DISPATCHER_CONTEXT ctx2{};
        ctx2.ContextRecord = &ctx;
        ctx2.HistoryTable = DispatcherContext->HistoryTable;
        ctx2.TargetIp = DispatcherContext->TargetIp;

        for (;;) {
            auto rt = RtlLookupFunctionEntry(ctx.Rip, &base_address, nullptr);
            if (!rt) {
                ctx.Rip = *(DWORD64*)ctx.Rsp;
                ctx.Rsp += sizeof(DWORD64);
                return;
            }
            ULONG64 establisher = 0;
            PVOID handler_data = nullptr;
            ctx2.ControlPc = unwind_ctx.Rip;
            auto routine = RtlVirtualUnwind(UNW_FLAG_EHANDLER, base_address, ctx.Rip, rt, &unwind_ctx, &handler_data, &establisher, nullptr);
            if (!routine) {
                ctx = unwind_ctx;
                continue;
            }
            if (EstablisherFrame == establisher) {
                ExceptionRecord->ExceptionFlags |= EXCEPTION_TARGET_UNWIND;
            }
            ctx2.HandlerData = handler_data;
            ctx2.ImageBase = base_address;
            ctx2.FunctionEntry = rt;
            ctx2.EstablisherFrame = establisher;
            ctx2.ScopeIndex = 0;
            unwind_ctx.Rax = (ULONG64)0;
            routine(ExceptionRecord, (void*)establisher, &ctx, DispatcherContext);
            if (EstablisherFrame == establisher) {
                return;
            }
            ctx = unwind_ctx;
        }
    }

    // see https://github.com/v8/v8/blob/9812cb486e2bfb7b72c7cee41d4ee1916dfef412/src/diagnostics/unwinding-info-win64.cc#L54
    int crash_handler(
        PEXCEPTION_RECORD ExceptionRecord, ULONG64 EstablisherFrame,
        PCONTEXT ContextRecord, PDISPATCHER_CONTEXT DispatcherContext) {
        auto entry = reinterpret_cast<JITUnwindInfo*>(DispatcherContext->FunctionEntry);
        if (entry->stack.null()) {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        if (ExceptionRecord->ExceptionCode == VCxxExceptionCode) {
            do_unwind(ExceptionRecord, EstablisherFrame, ContextRecord, DispatcherContext);
            auto base_address = DispatcherContext->ImageBase;
            auto func_info = (VCxx_FuncInfo*)(base_address + *(PULONG)DispatcherContext->HandlerData);
            auto destructors = (VCxx_UnwindMapEntry*)(base_address + func_info->disp_unwind_map);
            view::rspan<VCxx_UnwindMapEntry> unwind_map(destructors, size_t(func_info->max_state + 1));
            auto map_ = unwind_map[0];
        }
        auto stack_bottom = (jit::x64::coro::StackBottom*)(entry->stack.data() + entry->stack.size() - sizeof(jit::x64::coro::StackBottom));
        auto epilogue = stack_bottom->coroutine_epilogue_address;
        CrashInfo crash{
            .exception_record = ExceptionRecord,
            .establisher_frame = EstablisherFrame,
            .context_record = ContextRecord,
            .dispatcher_context = DispatcherContext,
        };
        stack_bottom->coroutine_epilogue_address = (std::uintptr_t)&crash;
        //
        stack_bottom->coroutine_epilogue_address = epilogue;
        // if fail, run the default handler (maybe memory leak)
        // set rax to &crash and jmp to epilogue
        asm volatile(
            "movq %0, %%rax\n"
            "jmpq *%1\n"
            :
            : "r"(&crash), "r"(epilogue)
            : "rax");
        __builtin_unreachable();
    }
    constexpr auto push_rbp_length = 1;
    constexpr auto mov_rbp_rsp_length = 3;
    constexpr auto rbp_prefix_codes = 2;
    constexpr auto rbp_prefix_length = push_rbp_length + mov_rbp_rsp_length;
    constexpr auto op_set_fp_reg = 3;
    constexpr auto op_push_nonvol = 0;

    // see https://github.com/v8/v8/blob/85ca7749ac8bd7dbc5c186f393d64e276471362f/src/diagnostics/unwinding-info-win64.cc#L158
    futils_DLL_EXPORT std::pair<size_t, bool> STDCALL prepare_jit_unwind(binary::writer& code) {
        code.prepare_stream(sizeof(JITUnwindInfo));
        auto code_memory = code.remain();
        auto current_offset = code.offset();
        if (code_memory.size() < sizeof(JITUnwindInfo)) {
            return {0, false};
        }
        code.offset(sizeof(JITUnwindInfo));
        auto memory = std::uint64_t(code_memory.data());
        auto rt = new (code_memory.data()) JITUnwindInfo{};
        rt->rt.BeginAddress = 0;                                   // will be updated later
        rt->rt.EndAddress = 0;                                     // will be updated later
        rt->rt.UnwindInfoAddress = offsetof(JITUnwindInfo, info);  // will be updated later
        rt->info.Version = 1;
        rt->info.Flags = std::uint8_t(UnwindFlag::EHANDLER);
        rt->info.SizeOfProlog = rbp_prefix_length;
        rt->info.CountOfCodes = rbp_prefix_codes;
        rt->info.FrameRegister = jit::x64::RBP;
        rt->info.FrameOffset = 0;
        rt->codes[0].CodeOffset = rbp_prefix_length;
        rt->codes[0].UnwindOp = op_set_fp_reg;
        rt->codes[0].OpInfo = 0;
        rt->codes[1].CodeOffset = push_rbp_length;
        rt->codes[1].UnwindOp = op_push_nonvol;
        rt->codes[1].OpInfo = jit::x64::RBP;
        rt->exception_handler = offsetof(JITUnwindInfo, exception_handler_);  // will be updated later
        auto handler = generate_jmp(reinterpret_cast<std::uintptr_t>(crash_handler));
        memcpy(rt->exception_handler_, handler.data(), handler.size());
        return {current_offset, true};
    }

    futils_DLL_EXPORT bool STDCALL apply_jit_unwind(view::wvec memory, size_t offset, view::wvec stack) {
        if (memory.size() < sizeof(JITUnwindInfo)) {
            return false;
        }
        if (offset + sizeof(JITUnwindInfo) > memory.size()) {
            return false;
        }
        auto rt = reinterpret_cast<JITUnwindInfo*>(memory.data() + offset);
        if (std::uintptr_t(rt) % 16 != 0) {
            return false;
        }
        if (rt->info.Version != 1 || rt->info.Flags != std::uint8_t(UnwindFlag::EHANDLER) ||
            rt->info.SizeOfProlog != 4 || rt->info.CountOfCodes != 2 ||
            rt->info.FrameRegister != jit::x64::RBP || rt->info.FrameOffset != 0) {
            return false;
        }
        if (rt->codes[0].CodeOffset != rbp_prefix_length ||
            rt->codes[0].UnwindOp != op_set_fp_reg ||
            rt->codes[0].OpInfo != 0 ||
            rt->codes[1].CodeOffset != push_rbp_length ||
            rt->codes[1].UnwindOp != op_push_nonvol ||
            rt->codes[1].OpInfo != jit::x64::RBP) {
            return false;
        }
        auto handler = generate_jmp(reinterpret_cast<std::uintptr_t>(crash_handler));
        if (memcmp(rt->exception_handler_, handler.data(), handler.size()) != 0) {
            return false;
        }
        // update addresses
        rt->rt.BeginAddress = offset;
        rt->rt.EndAddress = offset + memory.size();  // update end address
        rt->rt.UnwindInfoAddress = offset + offsetof(JITUnwindInfo, info);
        rt->exception_handler = offset + offsetof(JITUnwindInfo, exception_handler_);
        rt->stack = stack;
        return RtlAddFunctionTable(&rt->rt, 1, reinterpret_cast<std::uintptr_t>(memory.data())) == 1;
    }

}  // namespace futils::platform::windows
