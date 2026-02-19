/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <math/vector.h>

namespace futils::cg::camera {
    template <class T = double>
    struct Camera {
        math::Vec3<T> position;
        math::Vec3<T> look_at;
        math::Vec3<T> up;
    };
}  // namespace futils::cg::camera
