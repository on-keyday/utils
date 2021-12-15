/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// lite_lock - lite lock
#pragma once
#include <atomic>

namespace utils {
    namespace thread {
        struct LiteLock {
            std::atomic_flag flag;
            void lock() {
                while (flag.test_and_set() == true) {
                    flag.wait(true);
                }
            }

            void unlock() {
                flag.clear();
                flag.notify_all();
            }
        };

    }  // namespace thread
}  // namespace utils