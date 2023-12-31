/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "clock.h"

namespace utils::timer {

    struct TimerCallback {
       private:
        Time deadline;
        void (*callback)(void*) = nullptr;
        void* data = nullptr;

       public:
    };
}  // namespace utils::timer