/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../dll/dllh.h"
#include <cstdint>
#include <memory>
#include "../error.h"

namespace futils::fnet::event {

    constexpr static std::uintptr_t invalid_handle = -1;

    // program that uses Socket async operation must call this function finally
    // in common, this function is pass to make_io_event and then call this from IOEvent's wait method
    // on windows, ptr must be an pointer to OVERLAPPED_ENTRY
    // on linux, ptr must be an pointer to epoll_event
    // rt is ignored
    fnet_dll_export(void) fnet_handle_completion(void* ptr, void* rt);

    // Completion is completion object
    struct Completion {
        void* ptr = nullptr;
        std::uintptr_t data = 0;
    };

    fnet_dll_export(Completion) convert_completion(void* ptr);

    fnet_dll_export(void) fnet_handle_completion_via_Completion(Completion c);

    struct fnet_class_export IOEvent {
       private:
        std::uintptr_t handle = invalid_handle;
        void* rt = nullptr;
        void (*f)(void* ptr, void* rt) = nullptr;
        friend fnet_dll_export(expected<IOEvent>) make_io_event(void (*)(void*, void*), void*);

        constexpr IOEvent(std::uintptr_t h, void (*s)(void*, void*), void* r)
            : handle(h), f(s), rt(r) {}

       public:
        constexpr IOEvent() = default;
        constexpr IOEvent(IOEvent&& o)
            : handle(std::exchange(o.handle, invalid_handle)), f(std::exchange(o.f, nullptr)), rt(std::exchange(o.rt, nullptr)) {}

        expected<void> register_handle(std::uintptr_t handle, void* ptr);
        expected<size_t> wait(std::uint32_t timeout);
        ~IOEvent();
    };

    // on windows, ptr will be an pointer to OVERLAPPED_ENTRY
    // on linux, ptr will be an pointer to epoll_event
    // rt is pointer to runtime if specified by rt
    fnet_dll_export(expected<IOEvent>) make_io_event(void (*)(void* ptr, void* rt), void* rt);

}  // namespace futils::fnet::event
