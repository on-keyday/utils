/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <jit/x64_coroutine.h>
#include <jit/jit_memory.h>
#include <platform/windows/runtime_function.h>
#include <string>
#include <cstdio>

struct Destructor {
    ~Destructor() {
        printf("Destructor\n");
    }
};

int routine(int* (*suspend)(int), int* x) {
    Destructor d;
    for (int i = 0; i < 10; ++i) {
        *x = i;
        x = suspend(1);
        try {
            Destructor d;
            throw "runtime_error";
        } catch (...) {
            x = suspend(2);
        }
    }
    throw "bye bye";
    return 0;
}

int main() {
    std::string buffer;
    futils::binary::writer w(futils::binary::resizable_buffer_writer<std::string>(), &buffer);
    futils::byte stack_memory[1024 * 100]{};
#ifdef FUTILS_PLATFORM_WINDOWS
    auto [unwind_offset, _] = futils::platform::windows::prepare_jit_unwind(w);
#endif
    auto exec_offset = w.offset();
    auto reloc = futils::jit::x64::coro::write_coroutine(w, stack_memory, routine);
    auto edit = futils::jit::EditableMemory::allocate(buffer.size());
    memcpy(edit.get_memory().data(), buffer.data(), buffer.size());
    for (auto& r : reloc.relocations) {
        futils::jit::x64::coro::relocate(edit.get_memory(), r, stack_memory);
    }
#ifdef FUTILS_PLATFORM_WINDOWS
    futils::platform::windows::apply_jit_unwind(edit.get_memory(), unwind_offset, stack_memory);
#endif
    auto exec = edit.make_executable();
    auto f = exec.as_function<int, int*>(exec_offset);
    int x = 0;
    while (auto y = f(&x)) {
        if (y == 1) {
            printf("x = %d\n", x);
        }
        else if (y == 2) {
            printf("exception\n");
        }
    }
    printf("main end\n");
}