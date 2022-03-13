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
            size_t count;
            std::thread::id thread_id;
            void lock() {
                while (!try_lock()) {
                    if (thread_id == std::this_thread::get_id()) {
                        count++;
                        break;
                    }
                    flag.wait(true);
                }
                thread_id = std::this_thread::get_id();
            }

            bool try_lock() {
                return flag.test_and_set() == false;
            }

            void unlock() {
                if (thread_id == std::this_thread::get_id()) {
                    if (count) {
                        count--;
                    }
                    else {
                        flag.clear();
                        thread_id = {};
                    }
                }
                flag.notify_all();
            }
        };
    }  // namespace thread
}  // namespace utils
