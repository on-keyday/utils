/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <fnet/dll/dllcpp.h>
#include <fnet/event/event.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/epoll.h>
#endif
#include <fnet/dll/lazy/sockdll.h>
#include <fnet/dll/allocator.h>

namespace utils::fnet::event {
    fnet_dll_implement(std::optional<IOEvent>) make_io_event() {
        auto h = lazy::CreateIoCompletionPort_(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        if (h == nullptr) {
            return std::nullopt;
        }
        return IOEvent(std::uintptr_t(h));
    }

    void delete_handle::operator()(Handle* h) const {
        delete_glheap(h);
    }

    HandlePtr IOEvent::register_handle(std::uintptr_t handle) {
        if (lazy::CreateIoCompletionPort_(HANDLE(handle), HANDLE(this->handle), 0, 0) != HANDLE(handle)) {
            return nullptr;
        }
        auto h = new_glheap(Handle);
        h->handle = handle;
        h->w = new_glheap();
        return HandlePtr(h);
    }
}  // namespace utils::fnet::event
