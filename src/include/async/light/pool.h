/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// pool -  task pool object
#pragma once
#include "context.h"

namespace utils {
    namespace async {
        namespace light {
            struct ASYNC_NO_VTABLE_ Task {
                virtual bool run() = 0;
                virtual ~Task() {}
            };

            template <class T>
            struct FTask : Task {
                Future<T> task;
                bool run() override {
                    task.resume();
                    return task.done();
                }
            };
        }  // namespace light
    }      // namespace async
}  // namespace utils
