/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// recursive_lock - recursive lock
#pragma once
#include <atomic>
#include <thread>

namespace utils {
    namespace thread {
        struct RecursiveLock {
            std::atomic_flag flag;
            size_t count = 0;
            std::thread::id thread_id;
            void lock() {
                while (!try_lock()) {
                    flag.wait(true);
                }
            }

            bool try_lock() {
                if (flag.test_and_set() == false) {
                    thread_id = std::this_thread::get_id();
                    return true;
                }
                if (thread_id == std::this_thread::get_id()) {
                    count++;
                    return true;
                }
                return false;
            }

            void unlock() {
                if (thread_id == std::this_thread::get_id()) {
                    if (count) {
                        count--;
                    }
                    else {
                        thread_id = {};
                        flag.clear();
                    }
                }
                flag.notify_all();
            }
        };
    }  // namespace thread
}  // namespace utils
