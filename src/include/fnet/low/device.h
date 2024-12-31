/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <fnet/dll/dllh.h>
#include <view/iovec.h>
#include <memory>

namespace futils::fnet::low {
    enum class DefaultDeviceCmd {
        open,
        close,
        send,
        recv,
    };

    struct ResultProtocol {
        int result;
    };

    struct SendProtocol : ResultProtocol {
        view::rvec data;
    };

    struct RecvProtocol : ResultProtocol {
        view::wvec buffer;
    };

    struct OpenProtocol : ResultProtocol {
        int flags;
    };

    struct Device {
       private:
        void (*ctrl)(Device* self, int cmd, void* arg);

       public:
        constexpr Device(void (*ctrl)(Device* self, int cmd, void* arg))
            : ctrl(ctrl) {}
        Device(const Device&) = delete;
        Device(Device&&) = default;

        ~Device() {
            close();
        }

        void control(int cmd, void* arg) {
            if (ctrl) {
                ctrl(this, cmd, arg);
            }
        }

        int open(int flags) {
            OpenProtocol p{.flags = flags};
            control(static_cast<int>(DefaultDeviceCmd::open), const_cast<OpenProtocol*>(&p));
            return p.result;
        }

        void close() {
            control(static_cast<int>(DefaultDeviceCmd::close), nullptr);
        }

        int send(view::rvec data) {
            SendProtocol p{.data = data};
            control(static_cast<int>(DefaultDeviceCmd::send), const_cast<SendProtocol*>(&p));
            return p.result;
        }

        int recv(view::wvec buffer) {
            RecvProtocol p{.buffer = buffer};
            control(static_cast<int>(DefaultDeviceCmd::recv), const_cast<RecvProtocol*>(&p));
            return p.result;
        }
    };

    fnet_dll_export(Device*) network_devices(size_t* count);
}  // namespace futils::fnet::low
