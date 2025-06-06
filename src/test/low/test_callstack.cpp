/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <low/stack.h>
#include <cstddef>
#ifdef _WIN32
#include <windows.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;
const char* imageBaseAddr;

int filter(unsigned int code, struct _EXCEPTION_POINTERS* ep) {
    auto magic = __ImageBase.e_magic;
    auto ptr = (void*(*)[15])ep->ExceptionRecord->ExceptionInformation;
    return EXCEPTION_EXECUTE_HANDLER;
}

void f(futils::low::Stack* s, void* c) {
    auto stack = s->available_stack_span();
    auto ptr = stack.end();
    s->suspend();
    __try {
        // except(s);
    } __except (filter(GetExceptionCode(), GetExceptionInformation())) {
        s->resume();
    }
    // throw "";
}
#else
void f(futils::low::Stack* s, void* c) {
    auto stack = s->available_stack_span();
    auto ptr = stack.end();
    s->suspend();
    try {
        // except(s);
    } catch (...) {
        s->resume();
    }
    // throw "";
}
#endif

alignas(16) unsigned char data[1024 * 1024 + 16];

void except(futils::low::Stack* s) {
    throw "";
}

int main() {
    try {
        throw "";
    } catch (...) {
        ;
    }
    futils::low::Stack stack;
    stack.set(data, data + sizeof(data));
    auto span = stack.stack_span();
    span.back() = 0;
    stack.set(span.data(), span.end() - 2);
    stack.invoke(f, nullptr);
    // f(&stack, nullptr);
    auto span2 = stack.available_stack_span();
    stack.resume();
    try {
        except(&stack);
    } catch (...) {
        ;
    }
    for (auto& i : span) {
    }
}