/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// cond_writer - conditional writer
#pragma once
#include "appender.h"

namespace utils {
    namespace helper {
        namespace internal {
            template <class T, class V>
            struct FirstTimeWriter {
                T& buf;
                V later_write;
                bool first = true;
                void operator()(auto&&... v) {
                    if (!first) {
                        append(buf, later_write);
                    }
                    first = false;
                    appends(buf, v...);
                }
            };
        }  // namespace internal

        template <class T, class V>
        auto make_writer(T& t, V late) {
            return internal::FirstTimeWriter<T, std::decay_t<V>>{t, late};
        }
    }  // namespace helper
}  // namespace utils
