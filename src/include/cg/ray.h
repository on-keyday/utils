/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <math/vector.h>

namespace futils::cg::ray {
    template <class T = double>
    struct Ray {
        math::Vec3<T> origin;
        math::Vec3<T> direction;
    };
}  // namespace futils::cg::ray
