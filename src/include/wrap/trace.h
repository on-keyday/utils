/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <platform/windows/dllexport_header.h>
#include <helper/pushbacker.h>
#include <view/span.h>

namespace futils::wrap {
    struct futils_DLL_EXPORT stack_trace_entry {
       private:
        void* addr = nullptr;

       public:
        constexpr stack_trace_entry() = default;

        constexpr void* address() const {
            return addr;
        }

        void get_symbol(futils::helper::IPushBacker<> pb) const;
    };

    futils_DLL_EXPORT view::wspan<stack_trace_entry> STDCALL get_stack_trace(view::wspan<stack_trace_entry> entry);

    // if native is true, when windows info become pointer to SYMBOL_INFO
    futils_DLL_EXPORT void STDCALL get_symbols(view::rspan<stack_trace_entry> entry, void* p, void (*cb)(void* p, const char* info), bool native = false);

    template <class F>
    void get_symbols(view::rspan<stack_trace_entry> entry, F&& f, bool native = false) {
        auto addr = std::addressof(f);
        get_symbols(
            entry, addr, [](void* p, const char* info) {
                (*decltype(addr)(p))(info);
            },
            native);
    }
}  // namespace futils::wrap
