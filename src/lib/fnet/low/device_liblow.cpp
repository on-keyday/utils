/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <fnet/dll/dllcpp.h>
#include <fnet/low/device.h>
#include <fnet/dll/glheap.h>
#include <cstdlib>

namespace futils::fnet::low {
    void net_device_ctrl(Device* self, int cmd, void* arg);
    struct net_device : Device {
        constexpr net_device()
            : Device(net_device_ctrl) {}
    };

    net_device one_device;

}  // namespace futils::fnet::low
