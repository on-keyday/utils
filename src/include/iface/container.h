/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// container - container like interface
#pragma once
#include "base_iface.h"

namespace utils {
    namespace iface {
        template <class T, class Box>
        struct Queue : Box {
           private:
        };
    }  // namespace iface
}  // namespace utils