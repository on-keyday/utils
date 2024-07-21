/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/windows/dllexport_source.h>
#include <jit/jit_memory.h>
#include <platform/detect.h>
#ifdef FUTILS_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace futils::jit {
#ifdef FUTILS_PLATFORM_WINDOWS
    void* allocate_platform(size_t s) {
        return VirtualAlloc(nullptr, s, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }

    bool make_executable_platform(void* p, size_t s) {
        DWORD old_protect;
        return VirtualProtect(p, s, PAGE_EXECUTE_READ, &old_protect);
    }

    bool make_editable_platform(void* p, size_t s) {
        DWORD old_protect;
        return VirtualProtect(p, s, PAGE_READWRITE, &old_protect);
    }

    void free_platform(void* p, size_t s) {
        VirtualFree(p, s, MEM_RELEASE);
    }

#else
    void* allocate_platform(size_t s) {
        return mmap(nullptr, s, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }

    bool make_executable_platform(void* p, size_t s) {
        return mprotect(p, s, PROT_READ | PROT_EXEC) == 0;
    }

    bool make_editable_platform(void* p, size_t s) {
        return mprotect(p, s, PROT_READ | PROT_WRITE) == 0;
    }

    void free_platform(void* p, size_t s) {
        munmap(p, s);
    }
#endif
    EditableMemory EditableMemory::allocate(size_t size) {
        auto memory = allocate_platform(size);
        if (!memory) {
            return {};
        }
        return EditableMemory(view::wvec((byte*)memory, size));
    }

    ExecutableMemory EditableMemory::make_executable() {
        if (!valid()) {
            return {};
        }
        if (!make_executable_platform(memory.data(), memory.size())) {
            return {};
        }
        return ExecutableMemory(std::exchange(memory, view::wvec()));
    }

    EditableMemory ExecutableMemory::make_editable() {
        if (!valid()) {
            return {};
        }
        if (!make_editable_platform(memory.data(), memory.size())) {
            return {};
        }
        return EditableMemory(std::exchange(memory, view::wvec()));
    }

    void EditableMemory::free() {
        if (valid()) {
            free_platform(memory.data(), memory.size());
        }
        memory = view::wvec();
    }

    void ExecutableMemory::free() {
        if (valid()) {
            free_platform(memory.data(), memory.size());
        }
        memory = view::wvec();
    }
}  // namespace futils::jit
