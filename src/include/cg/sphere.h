/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <math/vector.h>
#include "binary/float.h"
#include "cg/base_object.h"
#include "ray.h"
#include "base_object.h"

namespace futils::cg::object {

    template <class T = double>
    struct Sphere {
        BaseObject base;
        math::Vec3<T> center;
        T radius;
    };

    template <class T>
    constexpr HitInfo<T> hit(const ray::Ray<T>& r, const Sphere<T>& s) {
        math::Vec3<T> oc = r.origin - s.center;
        auto a = r.direction.length_squared();
        auto b = 2 * dot(oc, r.direction);
        auto c = oc.length_squared() - s.radius * s.radius;
        auto discriminant = b * b - 4 * a * c;  // 判別式
        if (discriminant < 0) {
            return invalid_hit<T>;
        }
        auto sqrt_d = math::sqrt(discriminant);
        auto t_minus = (-b - sqrt_d) / (2.0 * a);

        // Return the smaller of the two positive roots
        if (t_minus > 0) return {binary::make_float(t_minus)};

        auto t_plus = (-b + sqrt_d) / (2.0 * a);
        if (t_plus > 0) return {binary::make_float(t_plus)};

        return invalid_hit<T>;
    }

    template <class T>
    constexpr auto normal(const NormalContext<T>& ctx, const Sphere<T>& t) {
        return (ctx.intersection_point - t.center);
    }

    namespace test {
        constexpr auto ray = ray::Ray<>{
            .origin = {0, 0, 0},
            .direction = math::Vec3<>{0, 0, -1}.normalize(),
        };
        constexpr auto sphere = Sphere<>{
            .center = {0, 0, -5},
            .radius = 2,
        };

        constexpr auto hit_detect = hit(ray, sphere);

        static_assert(hit_detect.hit_point.to_float() == 3);

        constexpr auto ray2 = ray::Ray<>{
            .origin = {0, 0, 0},
            .direction = math::Vec3<>{0, 0, -1}.normalize(),
        };
        constexpr auto sphere2 = Sphere<>{
            .center = {0, 0, 5},
            .radius = 2,
        };

        constexpr auto hit_detect2 = hit(ray2, sphere2);

        static_assert(hit_detect2.hit_point.is_nan());

    }  // namespace test
}  // namespace futils::cg::object
