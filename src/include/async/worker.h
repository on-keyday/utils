/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#pragma once
#include <thread>
#include "../thread/channel.h"
#include "../wrap/lite/map.h"
#include <functional>

namespace utils {
    namespace async {
        struct TaskState {
            int state = 0;
        };
        struct Worker {
            size_t id = 0;
            std::thread th;
            TaskState state;
        };

        struct TaskPool {
            wrap::map<size_t, Worker> worker;
            template <class Fn>
            void operator()(Fn&& fn) {
                std::function<void()> f;
            }
        };

    }  // namespace async
}  // namespace utils
