/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../dll/dllh.h"
#include <cstdint>
#include <optional>
#include <memory>

namespace utils::fnet::event {

    constexpr static std::uintptr_t invalid_handle = -1;

    struct Handle {
       private:
        std::uintptr_t handle = invalid_handle;
        void* w = nullptr;
        void* r = nullptr;
        friend struct IOEvent;

       public:
        constexpr std::uintptr_t native_handle() const {
            return handle;
        }
    };

    struct delete_handle {
        void operator()(Handle*) const;
    };

    using HandlePtr = std::unique_ptr<Handle, delete_handle>;

    struct fnet_class_export IOEvent {
       private:
        std::uintptr_t handle = invalid_handle;
        friend fnet_dll_export(std::optional<IOEvent>) make_io_event();

        constexpr IOEvent(std::uintptr_t h)
            : handle(h) {}

       public:
        constexpr IOEvent() = default;

        HandlePtr register_handle(std::uintptr_t handle);
        void wait();
        ~IOEvent();
    };

    fnet_dll_export(std::optional<IOEvent>) make_io_event();

}  // namespace utils::fnet::event
