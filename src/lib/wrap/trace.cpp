/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/windows/dllexport_source.h>
#include <wrap/trace.h>
#ifdef _WIN32
#include <windows.h>
#include <ImageHlp.h>
#include <mutex>
#include <helper/lock.h>
#pragma comment(lib, "imagehlp.lib")
#else
#include <execinfo.h>
#endif
#include <strutil/append.h>

namespace utils::wrap {
#ifdef _WIN32
    static std::mutex m;  // for multi thread DbgHelp func call

    static auto lock() {
        return utils::helper::lock(m);
    }

    union SymInfo {
        SYMBOL_INFO info;
        byte buffer[sizeof(SYMBOL_INFO) + 257]{};
    };

    void get_with_syminfo(SymInfo& info, void* addr) {
        info.info.SizeOfStruct = sizeof(SYMBOL_INFO);
        info.info.MaxNameLen = 256;
        {
            const auto l = lock();
            if (!SymFromAddr(GetCurrentProcess(), (DWORD64)addr, 0, &info.info)) {
                return;  // no symbol found
            }
        }
    }

    static bool maybe_init() {
        static auto sym = SymInitialize(GetCurrentProcess(), nullptr, true);
        return sym;
    }

#else
    static bool maybe_init() {
        return true;
    }

#endif

    void stack_trace_entry::get_symbol(helper::IPushBacker<> pb) const {
#ifdef _WIN32
        SymInfo info;
        get_with_syminfo(info, addr);
        strutil::append(pb, info.info.Name);
#else
        auto t = backtrace_symbols(&addr, 1);
        if (!t) {
            return;
        }
        const auto d = helper::defer([&] { free(t); });
        strutil::append(pb, t[0]);
#endif
    }

    view::wspan<stack_trace_entry> STDCALL get_stack_trace(view::wspan<stack_trace_entry> entry) {
        if (!maybe_init() || entry.size() == 0) {
            return {};
        }
#ifdef _WIN32
        auto len = RtlCaptureStackBackTrace(0, entry.size(), (void**)entry.data(), 0);
#else
        auto len = backtrace((void**)entry.data(), entry.size());
#endif
        return entry.substr(0, len);
    }

    void STDCALL get_symbols(view::rspan<stack_trace_entry> entry, void* p, void (*cb)(void* p, const char* info), bool native) {
#ifdef _WIN32
        for (auto ent : entry) {
            auto addr = ent.address();
            if (!addr) {
                cb(p, nullptr);
                continue;
            }
            SymInfo info;
            get_with_syminfo(info, addr);
            if (native) {
                cb(p, (const char*)info.buffer);
            }
            else {
                cb(p, info.info.Name);
            }
        }
#else
        auto p = backtrace_symbols((void**)entry.data(), entry.size());
        if (!t) {
            return;
        }
        const auto d = helper::defer([&] { free(t); });
        for (auto i = 0; i < entry.size(); i++) {
            cb(p, t[i]);
        }
#endif
    }

}  // namespace utils::wrap
