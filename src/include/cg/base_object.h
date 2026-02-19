/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <math/vector.h>
#include "binary/float.h"
#include "cg/ray.h"

namespace futils::cg::object {
    struct BaseObject {
        math::Vec3<std::uint8_t> color;
    };

    template <class Ty>
    struct HitInfo {
        binary::FloatT<Ty> hit_point;
        size_t index = 0;  // if needed
    };

    template <class Ty>
    constexpr auto invalid_hit = HitInfo<Ty>{.hit_point = binary::quiet_nan<Ty>};

    template <class Ty>
    struct NormalContext {
        ray::Ray<Ty> ray;
        HitInfo<Ty> hit_info;
        math::Vec3<Ty> intersection_point;
    };
}  // namespace futils::cg::object