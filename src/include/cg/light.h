/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <math/vector.h>

namespace futils::cg::light {

    template <class T = double>
    struct Light {
        math::Vec3<T> position;
        T intensity{};
    };
}  // namespace futils::cg::light
