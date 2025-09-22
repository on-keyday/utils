/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/windows/dllexport_source.h>
#include <platform/lazy_dll.h>
#include <view/iovec.h>
#include <helper/pushbacker.h>
#include <strutil/append.h>
#include <platform/detect.h>

#ifdef FUTILS_PLATFORM_WINDOWS
#include <windows.h>
#else
#if __has_include(<dlfcn.h>)
#include <dlfcn.h>
#define FNET_HAS_DLOPEN
#endif
#endif

namespace futils::platform::dll {

    func_t DLLInit::lookup(const char* name) {
        return dll ? dll->lookup_platform(name) : nullptr;
    }
    void (*call_fail)(const char* fn) = nullptr;
    futils_DLL_EXPORT void STDCALL set_call_fail_traits(void (*f)(const char* fn)) {
        call_fail = f;
    }
    futils_DLL_EXPORT void STDCALL call_fail_traits(const char* fn) {
        auto f = call_fail;
        if (f) {
            f(fn);
        }
        abort();
    }

    namespace internal {
        futils_DLL_EXPORT void STDCALL log_fn_load(const char* fn, bool ok) {
            char buf[120]{};
            helper::CharVecPushbacker<char> pb{buf, 119};
            strutil::appends(pb, fn, ok ? " loaded" : " unavailable", "\n");
#ifdef FUTILS_PLATFORM_WINDOWS
            OutputDebugStringA(buf);
#endif
        }
    }  // namespace internal
#ifdef FUTILS_PLATFORM_WINDOWS
    bool DLL::load_platform() {
        if (sys) {
            target = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        }
        else {
            target = LoadLibraryExW(path, 0, 0);
        }
        return target != nullptr;
    }

    void DLL::unload_platform() {
        if (!target) {
            return;
        }
        FreeLibrary((HMODULE)target);
        target = nullptr;
    }

    func_t DLL::lookup_platform(const char* func) {
        return func_t(GetProcAddress((HMODULE)target, func));
    }

    const char* platform_specific_error() {
        return nullptr;
    }
#elif defined(FNET_HAS_DLOPEN)

    bool DLL::load_platform() {
        if (sys) {
            target = RTLD_DEFAULT;
            return true;
        }
        target = dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
        return target != nullptr;
    }

    void DLL::unload_platform() {
        dlclose(target);
        target = nullptr;
    }

    func_t DLL::lookup_platform(const char* func) {
        return func_t(dlsym(target, func));
    }

    const char* platform_specific_error() {
        return dlerror();
    }
#else  // no dynamic loading
    bool DLL::load_platform() {
        return false;
    }

    void DLL::unload_platform() {
    }

    func_t DLL::lookup_platform(const char* func) {
        return nullptr;
    }
#endif

}  // namespace futils::platform::dll
