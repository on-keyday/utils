/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/event/io.h>
#include <fnet/dll/lazy/sockdll.h>
#include <fnet/dll/allocator.h>
#include <fnet/plthead.h>
#include <platform/detect.h>

namespace futils::fnet::event {
#ifdef FUTILS_PLATFORM_WINDOWS
    fnet_dll_implement(expected<IOEvent>) make_io_event(void (*f)(void*, void*), void* rt) {
        if (!f) {
            return unexpect(error::Error("need non-null pointer handler", error::Category::lib, error::fnet_usage_error));
        }
        auto h = lazy::CreateIoCompletionPort_(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        if (h == nullptr) {
            return unexpect(error::Errno());
        }
        return IOEvent(std::uintptr_t(h), f, rt);
    }

    expected<void> IOEvent::register_handle(std::uintptr_t handle, void* /*not used in windows*/) {
        if (lazy::CreateIoCompletionPort_(HANDLE(handle), HANDLE(this->handle), 0, 0) != HANDLE(this->handle)) {
            return unexpect(error::Errno());
        }
        return {};
    }

    expected<size_t> IOEvent::wait(std::uint32_t timeout) {
        OVERLAPPED_ENTRY ent[64];
        DWORD rem = 0;
        auto res = lazy::GetQueuedCompletionStatusEx_(HANDLE(handle), ent, 64, &rem, timeout, false);
        if (!res) {
            return unexpect(error::Errno());
        }
        int ev = 0;
        for (auto i = 0; i < rem; i++) {
            f(&ent[i], this->rt);
            ev++;
        }
        return ev;
    }

    IOEvent::~IOEvent() {
        if (handle != invalid_handle) {
            CloseHandle(HANDLE(handle));
        }
    }

    fnet_dll_implement(Completion) convert_completion(void* ptr) {
        auto ent = (OVERLAPPED_ENTRY*)ptr;
        return Completion{
            ent->lpOverlapped,
            ent->dwNumberOfBytesTransferred,
        };
    }

    fnet_dll_implement(void) fnet_handle_completion_via_Completion(Completion c) {
        OVERLAPPED_ENTRY ent;
        ent.lpOverlapped = (OVERLAPPED*)c.ptr;
        ent.dwNumberOfBytesTransferred = c.data;
        fnet_handle_completion(&ent, nullptr);
    }

#elif defined(FUTILS_PLATFORM_LINUX)
    fnet_dll_implement(expected<IOEvent>) make_io_event(void (*f)(void*, void*), void* rt) {
        if (!f) {
            return unexpect(error::Error("need non-null pointer handler", error::Category::lib, error::fnet_usage_error));
        }
        auto h = lazy::epoll_create1_(EPOLL_CLOEXEC);
        if (h == -1) {
            return unexpect(error::Errno());
        }
        return IOEvent(std::uintptr_t(h), f, rt);
    }

    expected<void> IOEvent::register_handle(std::uintptr_t handle, void* ptr) {
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLEXCLUSIVE;
        ev.data.ptr = ptr;
        if (lazy::epoll_ctl_(this->handle, EPOLL_CTL_ADD, handle, &ev) != 0) {
            return unexpect(error::Errno());
        }
        return {};
    }

    expected<size_t> IOEvent::wait(std::uint32_t timeout) {
        int t = timeout;
        epoll_event ev[64]{};
        sigset_t set;
        auto res = lazy::epoll_pwait_(handle, ev, 64, t, &set);
        if (res == -1) {
            return unexpect(error::Errno());
        }
        int proc = 0;
        for (auto i = 0; i < res; i++) {
            f(&ev[i], rt);
        }
        return proc;
    }

    IOEvent::~IOEvent() {
        lazy::close_(handle);
    }

    fnet_dll_export(Completion) convert_completion(void* ptr) {
        auto ent = (epoll_event*)ptr;
        return Completion{
            ent->data.ptr,
            ent->events,
        };
    }

    fnet_dll_implement(void) fnet_handle_completion_via_Completion(Completion c) {
        epoll_event evt;
        evt.data.ptr = c.ptr;
        evt.events = std::uint32_t(c.data);
        fnet_handle_completion(&evt, nullptr);
    }
#endif
}  // namespace futils::fnet::event
