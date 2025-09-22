/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "base_object.h"
#include "binary/float.h"
#include "cg/base_object.h"
#include "math/vector.h"
#include "ray.h"
#include "light.h"
#include "any_object.h"

namespace futils::cg::object {

    template <class T>
    constexpr auto shading(const HitInfo<T>& hit_point, const ray::Ray<T>& ray, const AnyObject<T>& object, const light::Light<T>& light) {
        math::Vec3<T> point = ray.origin + hit_point.hit_point.to_float() * ray.direction;
        NormalContext<T> ctx{
            .ray = ray,
            .hit_info = hit_point,
            .intersection_point = point,
        };
        math::Vec3<T> normal_vec = normal(ctx, object).normalize(1000);
        math::Vec3<T> light_vec = (light.position - point).normalize(1000);
        auto brightness = binary::make_float(dot(normal_vec, light_vec));
        if (brightness.is_nan()) {
            return brightness;
        }
        if (brightness.sign()) {
            return binary::zero<T>;
        }
        return binary::make_float(brightness.to_float() * light.intensity);
    }
}  // namespace futils::cg::object
