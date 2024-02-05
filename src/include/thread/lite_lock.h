/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// lite_lock - lite lock
#pragma once
#include <atomic>

namespace futils {
    namespace thread {
        struct LiteLock {
            std::atomic_flag flag;

            void lock() {
                while (!try_lock()) {
                    flag.wait(true);
                }
            }

            bool try_lock() {
                return flag.test_and_set(std::memory_order_acquire) == false;
            }

            void unlock() {
                flag.clear(std::memory_order_release);
                flag.notify_all();
            }
        };

    }  // namespace thread
}  // namespace futils
