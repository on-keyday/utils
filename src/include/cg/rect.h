/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <math/vector.h>

namespace futils::cg::rect {
    template <class T>
    struct Rect {
        math::Vec2<T> position;
        math::Vec2<T> size;
    };
}  // namespace futils::cg::rect
