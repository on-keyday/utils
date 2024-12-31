/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../dll/dllh.h"
#include <cstdint>
#include <memory>
#include "../error.h"

namespace futils::fnet {
    namespace event {

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

        // convert platform specific completion object to Completion object
        // on windows, ptr must be an pointer to OVERLAPPED_ENTRY
        // on linux, ptr must be an pointer to epoll_event
        // this function is purposed to use on IOEvent's callback
        fnet_dll_export(Completion) convert_completion(void* ptr);

        // program that uses Socket async operation must call this function finally if it uses Completion object
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

        /**
         * @brief Creates a new I/O event for asynchronous operations.
         *
         * This function initializes an I/O event object that will be used for
         * handling asynchronous I/O operations.
         *
         * @param completionHandler A function pointer for handling completion
         *        events. The function signature should match:
         *        `void (*)(void* ptr, void* rt)`.
         *
         *        - On Windows: `ptr` will point to an `OVERLAPPED_ENTRY`.
         *        - On Linux: `ptr` will point to an `epoll_event`.
         *
         *        Note: The object referred to by `ptr` is allocated on the
         *        stack. Therefore, it must not be used after this function
         *        call returns. To utilize the object outside this function,
         *        convert `ptr` to a `Completion` object using `convert_completion`.
         *
         * @param rt An optional pointer to the runtime context, which can
         *        be specified if needed.
         *
         * @return An expected `IOEvent` object, which contains the initialized
         *         I/O event for further operations.
         */
        fnet_dll_export(expected<IOEvent>) make_io_event(void (*)(void* ptr, void* rt), void* rt);
    }  // namespace event

    // wait_event waits io completion until time passed
    // if event is processed function returns number of event
    // otherwise returns 0
    // this call GetQueuedCompletionStatusEx on windows
    // or call epoll_wait on linux
    // this is based on default IOEvent
    // so if you uses custom IOEvent, use it's wait method instead
    fnet_dll_export(expected<size_t>) wait_io_event(std::uint32_t time);
}  // namespace futils::fnet
