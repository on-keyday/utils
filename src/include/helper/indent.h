/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// indent - indent utility
#pragma once
#include "appender.h"

namespace utils {
    namespace helper {
        template <class T, class Indent = const char*>
        struct IndentWriter {
            T t;
            Indent indent;
            template <class V, class C>
            constexpr IndentWriter(V&& v, C&& c)
                : t(v), indent(c) {}
        };
    }  // namespace helper
}  // namespace utils
