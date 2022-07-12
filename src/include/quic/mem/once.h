/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// once - initialize once
#pragma once
#include <mutex>
#include <atomic>

namespace utils {
    namespace quic {
        namespace mem {

            struct Once {
                std::mutex m;
                std::atomic_flag inited;

                void Do(auto&& callback) {
                    if (inited.test()) {
                        return;
                    }
                    std::scoped_lock lock{m};
                    if (inited.test()) {
                        return;
                    }
                    callback();
                    inited.test_and_set();
                }

                void Undo(auto&& callback) {
                    if (!inited.test()) {
                        return;
                    }
                    std::scoped_lock lock{m};
                    if (!inited.test()) {
                        return;
                    }
                    callback();
                    inited.clear();
                }
            };

        }  // namespace mem
    }      // namespace quic
}  // namespace utils