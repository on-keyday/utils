/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <platform/windows/dllexport_header.h>
#include <cstdint>
#include <view/span.h>
#include <binary/writer.h>
#include <utility>

// see https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170
namespace futils::platform::windows {
    struct RuntimeFunction {
        std::uint32_t BeginAddress;
        std::uint32_t EndAddress;
        std::uint32_t UnwindInfoAddress;
    };

    struct UnwindCode {
        std::uint8_t CodeOffset;
        std::uint8_t UnwindOp : 4;
        std::uint8_t OpInfo : 4;
    };

    enum class UnwindFlag : std::uint8_t {
        NHANDLER = 0,
        EHANDLER = 1,
        UHANDLER = 2,
        CHAININFO = 4
    };

    struct ExceptionHandlerData {
        std::uint32_t begin_address;

        void *language_specific_data() {
            return reinterpret_cast<void *>(this + 1);
        }
    };

    futils_DLL_EXPORT void *get_exception_handler_address(const ExceptionHandlerData &eh);

    struct UnwindInfoHeader {
        std::uint8_t Version : 3;
        std::uint8_t Flags : 5;
        std::uint8_t SizeOfProlog;
        std::uint8_t CountOfCodes;
        std::uint8_t FrameRegister : 4;
        std::uint8_t FrameOffset : 4;

        view::rspan<UnwindCode> get_unwind_code() {
            return view::rspan<UnwindCode>(reinterpret_cast<UnwindCode *>(this + 1), size_t(CountOfCodes));
        }

        ExceptionHandlerData *get_exception_handler_data() {
            if ((Flags & static_cast<std::uint8_t>(UnwindFlag::EHANDLER)) == 0 &&
                (Flags & static_cast<std::uint8_t>(UnwindFlag::UHANDLER)) == 0) {
                return nullptr;
            }
            auto ptr = reinterpret_cast<ExceptionHandlerData *>(reinterpret_cast<std::uintptr_t>(this) + sizeof(UnwindInfoHeader) + CountOfCodes * sizeof(UnwindCode));
            return ptr;
        }

        RuntimeFunction *get_chained_function_entry() {
            if ((Flags & static_cast<std::uint8_t>(UnwindFlag::CHAININFO)) == 0) {
                return nullptr;
            }
            auto ptr = reinterpret_cast<RuntimeFunction *>(reinterpret_cast<std::uintptr_t>(this) + sizeof(UnwindInfoHeader) + CountOfCodes * sizeof(UnwindCode));
            return ptr;
        }
    };

    struct RealRuntimeFunction {
        void *BeginAddress;
        void *EndAddress;
        UnwindInfoHeader *UnwindInfoAddress;
    };

    futils_DLL_EXPORT view::rspan<RuntimeFunction> STDCALL get_runtime_functions();

    futils_DLL_EXPORT RealRuntimeFunction STDCALL get_real_runtime_functions(const RuntimeFunction &rf);

    // see https://github.com/v8/v8/blob/85ca7749ac8bd7dbc5c186f393d64e276471362f/src/diagnostics/unwinding-info-win64.cc#L158
    futils_DLL_EXPORT std::pair<size_t, bool> STDCALL prepare_jit_unwind(binary::writer &code);
    futils_DLL_EXPORT bool STDCALL apply_jit_unwind(view::wvec memory, size_t offset, view::wvec stack = {});

    struct CrashInfo {
        void *exception_record = nullptr;
        std::uintptr_t establisher_frame = 0;
        void *context_record = nullptr;
        void *dispatcher_context = nullptr;
    };

    struct VCxx_ThrowInfo {
        std::uint32_t attributes;
        std::int32_t destructor_fn;
        std::int32_t forward_compat;
        std::int32_t catchable_type;
    };

    struct VCxx_FuncInfo {
        std::uint32_t magic_number : 29;
        std::uint32_t has_seh : 1;
        std::uint32_t has_exception_handler : 1;
        std::uint32_t has_type_info : 1;
        std::int32_t max_state;
        std::int32_t disp_unwind_map;
        std::uint32_t try_block_count;
        std::int32_t disp_try_block_map;
        std::uint32_t ip_map_count;
        std::int32_t disp_ip_map;
        std::int32_t disp_unwind_help;
        std::int32_t disp_es_type_list;
        std::int32_t eh_flags;
    };

    struct VCxx_UnwindMapEntry {
        std::int32_t to_state;
        std::int32_t action;
    };

    // see http://support.microsoft.com/kb/185294 for more information
    const std::uint32_t VCxxExceptionCode = 0xe06d7363;
}  // namespace futils::platform::windows
